"""
Base agent class for all agent types in the hierarchy.
"""
from abc import ABC, abstractmethod
from typing import Optional
import asyncio
from queue import Queue, Empty

from ..message import Message, Response


class BaseAgent(ABC):
    """
    Base class for all agents in the system.

    Agents communicate via messages and can process commands asynchronously.
    """

    def __init__(self, agent_id: str):
        self.agent_id = agent_id
        self.inbox: Queue = Queue()
        self.is_running = False

    @abstractmethod
    async def process_message(self, message: Message) -> Response:
        """
        Process an incoming message and return a response.

        Args:
            message: The message to process

        Returns:
            Response to the message
        """
        pass

    def send_message(self, message: Message, target_agent: "BaseAgent") -> None:
        """
        Send a message to another agent.

        Args:
            message: The message to send
            target_agent: The agent to send the message to
        """
        target_agent.receive_message(message)

    def receive_message(self, message: Message) -> None:
        """
        Receive a message into the inbox.

        Args:
            message: The message to receive
        """
        self.inbox.put(message)

    def get_message(self, timeout: Optional[float] = None) -> Optional[Message]:
        """
        Get the next message from the inbox.

        Args:
            timeout: How long to wait for a message (None = don't wait)

        Returns:
            The next message, or None if no message available
        """
        try:
            return self.inbox.get(block=True, timeout=timeout or 0.1)
        except Empty:
            return None

    async def run(self) -> None:
        """
        Run the agent's message processing loop.
        """
        self.is_running = True
        while self.is_running:
            message = self.get_message(timeout=0.1)
            if message:
                await self.process_message(message)
            await asyncio.sleep(0.01)  # Small delay to prevent busy-waiting

    def stop(self) -> None:
        """Stop the agent's message processing loop."""
        self.is_running = False
