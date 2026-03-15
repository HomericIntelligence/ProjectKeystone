from __future__ import annotations

import logging

from keystone.dag_walker import DAGWalker
from keystone.maestro_client import MaestroClient
from keystone.models import Agent, Task, TaskStatus

logger = logging.getLogger(__name__)


class TaskClaimer:
    """Orchestrates DAG advancement: fetch tasks → find ready → claim for agents."""

    def __init__(self, maestro_client: MaestroClient) -> None:
        self._client = maestro_client

    async def claim_task(self, team_id: str, task_id: str, agent_id: str) -> tuple[bool, list[Task]]:
        """PUT /api/teams/{id}/tasks/{taskId} → status=in_progress, assignee_agent_id=agent_id.

        Returns (success, unblocked_tasks). unblocked_tasks is the list of tasks that just
        became unblocked as reported by ai-maestro's PUT response.
        """
        try:
            _task, unblocked = await self._client.update_task(
                team_id=team_id,
                task_id=task_id,
                status=TaskStatus.IN_PROGRESS,
                assignee_agent_id=agent_id,
            )
            logger.info(
                "Claimed task %s for agent %s in team %s", task_id, agent_id, team_id
            )
            return True, unblocked
        except Exception as exc:
            logger.error(
                "Failed to claim task %s for agent %s: %s", task_id, agent_id, exc
            )
            return False, []

    async def get_available_agents(self) -> list[Agent]:
        """GET /api/agents/unified → filter for active+online, unoccupied agents."""
        try:
            agents = await self._client.get_agents()
        except Exception as exc:
            logger.error("Failed to fetch agents: %s", exc)
            return []

        available = [
            a
            for a in agents
            if a.status == "active" and a.session_status == "online"
        ]
        logger.debug("Found %d available agents (of %d total)", len(available), len(agents))
        return available

    async def advance_dag(self, team_id: str) -> list[str]:
        """Advance the DAG for a team by claiming all newly unblocked tasks.

        Steps:
        1. GET all tasks for the team.
        2. Find ready tasks (deps all COMPLETED, status PENDING).
        3. GET available agents.
        4. Claim each ready task for an available agent (round-robin).
        5. Return list of claimed task IDs.
        """
        logger.info("Advancing DAG for team %s", team_id)

        # 1. Fetch tasks
        try:
            task_list = await self._client.get_tasks(team_id)
        except Exception as exc:
            logger.error("Failed to fetch tasks for team %s: %s", team_id, exc)
            return []

        tasks: dict[str, Task] = {t.id: t for t in task_list}

        # 2. Find ready tasks
        try:
            ready = DAGWalker.find_ready_tasks(tasks)
        except Exception as exc:
            logger.error("DAG traversal failed for team %s: %s", team_id, exc)
            return []

        if not ready:
            logger.debug("No ready tasks found for team %s", team_id)
            return []

        logger.info("Found %d ready task(s) for team %s", len(ready), team_id)

        # 3. Get available agents
        agents = await self.get_available_agents()
        if not agents:
            logger.warning("No available agents — skipping claim for team %s", team_id)
            return []

        # 4. Claim ready tasks round-robin across available agents
        claimed: list[str] = []
        for task, agent in zip(ready, agents):
            success, _unblocked = await self.claim_task(team_id, task.id, agent.id)
            if success:
                claimed.append(task.id)

        logger.info(
            "Claimed %d/%d ready task(s) for team %s", len(claimed), len(ready), team_id
        )
        return claimed
