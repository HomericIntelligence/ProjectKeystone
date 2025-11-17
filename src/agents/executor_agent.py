"""
Executor Agent (Agent 2) - executes bash commands sent by commander agents.

This is a simplified Level 3 Task Agent for Phase 1 testing.
"""
import subprocess
from typing import Optional

from .base_agent import BaseAgent
from ..message import Message, Response


class ExecutorAgent(BaseAgent):
    """
    ExecutorAgent receives commands from commander agents and executes them.

    In Phase 1, this acts as a Task Agent that can run bash commands.
    """

    def __init__(self, agent_id: str = "executor_1"):
        super().__init__(agent_id)
        self.command_log: list[tuple[str, str]] = []  # (command, result) pairs

    async def process_message(self, message: Message) -> Response:
        """
        Process incoming command messages.

        For ExecutorAgent, this means executing the bash command and returning the result.
        """
        try:
            command = message.command

            # Execute the bash command
            result = self._execute_bash(command)

            # Log the execution
            self.command_log.append((command, result))

            # Send response back to commander
            response = Response.create_success(message, self.agent_id, result)

            # Send response as a message back to the sender
            response_msg = Message.create(
                sender_id=self.agent_id,
                receiver_id=message.sender_id,
                command="response",
                payload={"response": response, "original_command": command}
            )
            response_msg.msg_id = message.msg_id  # Keep same msg_id for tracking

            # Find sender agent and send response
            # Note: In a real system, this would use a message bus
            # For now, we'll store the response in the message payload
            self.outbox = response_msg

            return response

        except Exception as e:
            # Create error response
            response = Response.create_error(message, self.agent_id, str(e))

            # Send error response as a message back to the sender
            response_msg = Message.create(
                sender_id=self.agent_id,
                receiver_id=message.sender_id,
                command="response",
                payload={"response": response, "original_command": message.command}
            )
            response_msg.msg_id = message.msg_id  # Keep same msg_id for tracking

            # Store in outbox
            self.outbox = response_msg

            return response

    def _execute_bash(self, command: str) -> str:
        """
        Execute a bash command and return the result.

        Args:
            command: The bash command to execute

        Returns:
            The stdout output of the command

        Raises:
            subprocess.CalledProcessError: If the command fails
        """
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                timeout=10  # 10 second timeout
            )

            # Check for errors
            if result.returncode != 0:
                error_msg = result.stderr or f"Command exited with code {result.returncode}"
                raise subprocess.CalledProcessError(result.returncode, command, result.stdout, error_msg)

            return result.stdout.strip()

        except subprocess.TimeoutExpired:
            raise TimeoutError(f"Command '{command}' timed out after 10 seconds")

    def get_command_history(self) -> list[tuple[str, str]]:
        """Get the history of executed commands and their results."""
        return self.command_log.copy()
