"""
Commander Agent (Agent 1) - sends commands to executor agents.

This is a simplified Level 0 agent for Phase 1 testing.
"""
from typing import Optional
import asyncio

from .base_agent import BaseAgent
from ..message import Message, Response


class CommanderAgent(BaseAgent):
    """
    CommanderAgent sends commands to executor agents and waits for results.

    In Phase 1, this acts as the Chief Architect agent in the simplest form.
    """

    def __init__(self, agent_id: str = "commander_1"):
        super().__init__(agent_id)
        self.pending_responses: dict[str, asyncio.Future] = {}

    async def process_message(self, message: Message) -> Response:
        """
        Process incoming messages.

        For CommanderAgent, these are typically responses from executor agents.
        """
        # Check if this is a response to a pending command
        if message.msg_id in self.pending_responses:
            future = self.pending_responses[message.msg_id]
            # Extract result from message payload
            if message.payload and "response" in message.payload:
                future.set_result(message.payload["response"])
            else:
                future.set_result(message.payload)
            del self.pending_responses[message.msg_id]

        # Return empty response (commander doesn't need to respond to responses)
        return Response.create_success(message, self.agent_id, None)

    async def send_command(
        self,
        command: str,
        executor: BaseAgent,
        payload: Optional[dict] = None,
        timeout: float = 5.0
    ) -> Response:
        """
        Send a command to an executor agent and wait for the response.

        Args:
            command: The command string to send
            executor: The executor agent to send the command to
            payload: Optional additional data
            timeout: How long to wait for a response

        Returns:
            The response from the executor
        """
        # Create message
        msg = Message.create(
            sender_id=self.agent_id,
            receiver_id=executor.agent_id,
            command=command,
            payload=payload
        )

        # Create a future to track the response
        response_future = asyncio.Future()
        self.pending_responses[msg.msg_id] = response_future

        # Send the message
        self.send_message(msg, executor)

        # Wait for response with timeout
        try:
            result = await asyncio.wait_for(response_future, timeout=timeout)
            return result
        except asyncio.TimeoutError:
            # Clean up pending response
            if msg.msg_id in self.pending_responses:
                del self.pending_responses[msg.msg_id]
            raise TimeoutError(f"Command '{command}' timed out after {timeout}s")
