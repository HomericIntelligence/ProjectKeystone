"""
E2E Test for Phase 1: Basic Delegation

Tests the simplest 2-agent hierarchy:
- CommanderAgent (Level 0) sends a command
- ExecutorAgent (Level 3) executes the command and returns result

Test Case:
    Commander sends a bash command to add two randomly selected numbers,
    and Executor returns the result.
"""
import pytest
import asyncio
import random
from src.agents.commander_agent import CommanderAgent
from src.agents.executor_agent import ExecutorAgent
from src.message import Message


@pytest.mark.asyncio
async def test_commander_sends_addition_command_to_executor():
    """
    E2E Test: Commander sends bash command to add two random numbers.

    Flow:
        1. Commander generates two random numbers
        2. Commander sends bash command: "echo $((num1 + num2))"
        3. Executor receives command
        4. Executor runs bash command
        5. Executor sends result back to Commander
        6. Commander receives result
        7. Assert result is correct
    """
    # ARRANGE: Create agents
    commander = CommanderAgent("commander_1")
    executor = ExecutorAgent("executor_1")

    # Generate two random numbers
    num1 = random.randint(1, 100)
    num2 = random.randint(1, 100)
    expected_result = num1 + num2

    print(f"\nTest: Adding {num1} + {num2} = {expected_result}")

    # Create bash command to add the numbers
    bash_command = f"echo $(({num1} + {num2}))"

    # ACT: Start executor in background
    executor_task = asyncio.create_task(executor.run())

    # Wait a moment for executor to start
    await asyncio.sleep(0.1)

    # Send command from commander to executor
    # We need to handle this synchronously for the test
    msg = Message.create(
        sender_id=commander.agent_id,
        receiver_id=executor.agent_id,
        command=bash_command
    )

    # Send message to executor
    executor.receive_message(msg)

    # Wait for executor to process (give it time to execute)
    await asyncio.sleep(0.5)

    # Check executor's outbox for response
    response_msg = executor.outbox

    # ASSERT: Verify the result
    assert response_msg is not None, "Executor should have sent a response"
    assert response_msg.payload is not None, "Response should have payload"
    assert "response" in response_msg.payload, "Response payload should contain 'response'"

    response = response_msg.payload["response"]
    assert response.status == "success", f"Command should succeed, got: {response.error}"
    assert response.result == str(expected_result), \
        f"Expected {expected_result}, got {response.result}"

    print(f"✓ Executor correctly computed: {num1} + {num2} = {response.result}")

    # Verify command was logged
    history = executor.get_command_history()
    assert len(history) == 1, "Executor should have executed exactly one command"
    assert history[0][0] == bash_command, "Command in history should match"
    assert history[0][1] == str(expected_result), "Result in history should match"

    # CLEANUP
    executor.stop()
    await executor_task


@pytest.mark.asyncio
async def test_multiple_sequential_commands():
    """
    E2E Test: Commander sends multiple commands sequentially to Executor.
    """
    # ARRANGE
    commander = CommanderAgent("commander_1")
    executor = ExecutorAgent("executor_1")

    # Start executor
    executor_task = asyncio.create_task(executor.run())
    await asyncio.sleep(0.1)

    # ACT: Send three addition commands
    test_cases = [
        (5, 3, 8),
        (15, 27, 42),
        (100, 200, 300)
    ]

    for num1, num2, expected in test_cases:
        bash_command = f"echo $(({num1} + {num2}))"

        msg = Message.create(
            sender_id=commander.agent_id,
            receiver_id=executor.agent_id,
            command=bash_command
        )

        executor.receive_message(msg)
        await asyncio.sleep(0.3)

        response_msg = executor.outbox
        assert response_msg is not None

        response = response_msg.payload["response"]
        assert response.status == "success"
        assert response.result == str(expected), \
            f"Expected {expected}, got {response.result}"

        print(f"✓ {num1} + {num2} = {response.result}")

    # ASSERT: Verify all commands in history
    history = executor.get_command_history()
    assert len(history) == 3, "Should have executed 3 commands"

    # CLEANUP
    executor.stop()
    await executor_task


@pytest.mark.asyncio
async def test_executor_handles_command_errors():
    """
    E2E Test: Executor handles invalid bash commands gracefully.
    """
    # ARRANGE
    commander = CommanderAgent("commander_1")
    executor = ExecutorAgent("executor_1")

    executor_task = asyncio.create_task(executor.run())
    await asyncio.sleep(0.1)

    # ACT: Send invalid command
    invalid_command = "this_command_does_not_exist"

    msg = Message.create(
        sender_id=commander.agent_id,
        receiver_id=executor.agent_id,
        command=invalid_command
    )

    executor.receive_message(msg)
    await asyncio.sleep(0.3)

    # ASSERT: Should get error response
    response_msg = executor.outbox
    assert response_msg is not None

    response = response_msg.payload["response"]
    assert response.status == "error", "Invalid command should return error status"
    assert response.error is not None, "Error response should contain error message"

    print(f"✓ Executor correctly handled invalid command with error: {response.error}")

    # CLEANUP
    executor.stop()
    await executor_task


if __name__ == "__main__":
    # Run tests manually for debugging
    asyncio.run(test_commander_sends_addition_command_to_executor())
    print("\n" + "="*60 + "\n")
    asyncio.run(test_multiple_sequential_commands())
    print("\n" + "="*60 + "\n")
    asyncio.run(test_executor_handles_command_errors())
