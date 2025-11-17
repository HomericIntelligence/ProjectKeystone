"""
Simple message protocol for agent communication.
This implements a basic version of the KIM (Keystone Interchange Message) protocol.
"""
from dataclasses import dataclass
from typing import Any, Optional
import uuid
from datetime import datetime


@dataclass
class Message:
    """
    A message passed between agents.

    For Phase 1, this is a simple text-based command/response protocol.
    """
    msg_id: str
    sender_id: str
    receiver_id: str
    command: str  # The text command to execute
    payload: Optional[dict[str, Any]] = None  # Optional additional data
    timestamp: Optional[str] = None

    def __post_init__(self):
        if self.timestamp is None:
            self.timestamp = datetime.utcnow().isoformat()

    @staticmethod
    def create(sender_id: str, receiver_id: str, command: str, payload: Optional[dict] = None) -> "Message":
        """Factory method to create a new message with auto-generated ID."""
        return Message(
            msg_id=str(uuid.uuid4()),
            sender_id=sender_id,
            receiver_id=receiver_id,
            command=command,
            payload=payload or {}
        )


@dataclass
class Response:
    """
    A response to a message.
    """
    msg_id: str  # ID of the original message this responds to
    sender_id: str
    receiver_id: str
    status: str  # "success" or "error"
    result: Any  # The result of executing the command
    error: Optional[str] = None
    timestamp: Optional[str] = None

    def __post_init__(self):
        if self.timestamp is None:
            self.timestamp = datetime.utcnow().isoformat()

    @staticmethod
    def create_success(original_msg: Message, sender_id: str, result: Any) -> "Response":
        """Create a success response."""
        return Response(
            msg_id=original_msg.msg_id,
            sender_id=sender_id,
            receiver_id=original_msg.sender_id,
            status="success",
            result=result
        )

    @staticmethod
    def create_error(original_msg: Message, sender_id: str, error: str) -> "Response":
        """Create an error response."""
        return Response(
            msg_id=original_msg.msg_id,
            sender_id=sender_id,
            receiver_id=original_msg.sender_id,
            status="error",
            result=None,
            error=error
        )
