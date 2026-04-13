"""Pytest configuration and fixtures for DCC loopback tests."""

import pytest
from dcc_test_fixture import DCCTestFixture


def pytest_addoption(parser):
    parser.addoption("--cmd-port", required=True,
                     help="Serial port for Board A (command station)")
    parser.addoption("--dec-port", required=True,
                     help="Serial port for Board B (decoder)")


@pytest.fixture(scope="session")
def dcc(request):
    """Session-scoped fixture: opens serial ports, powers on, yields, powers off."""
    cmd_port = request.config.getoption("--cmd-port")
    dec_port = request.config.getoption("--dec-port")

    fixture = DCCTestFixture(cmd_port, dec_port)

    # Disable auto-refresh for deterministic one-shot packets
    fixture.set_refresh(False)

    # Power on the track
    resp = fixture.power_on()
    assert resp.startswith("OK"), f"POWER ON failed: {resp}"

    yield fixture

    fixture.power_off()
    fixture.close()


@pytest.fixture(autouse=True)
def clear_between_tests(dcc):
    """Clear decoder RECV buffer before each test."""
    dcc.clear_decoder()


@pytest.fixture(scope="class")
def svc_mode(dcc):
    """Enter service mode before the test, exit after.

    Usage: add 'svc_mode' to the test parameter list or use autouse
    in a service mode test class.
    """
    dcc.power_off()
    resp = dcc.svc_enter()
    assert resp.startswith("OK"), f"SVC ENTER failed: {resp}"
    yield dcc
    dcc.svc_exit()
    dcc.power_on()
