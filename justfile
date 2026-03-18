# ProjectKeystone justfile — wraps pixi + CMake workflow
# See also: Makefile (C++ build), pixi.toml (dependency management)

default:
    @just --list

# ---- C++ (via pixi) ----

configure:
    pixi run configure

build:
    pixi run build

test:
    pixi run test

clean:
    pixi run clean

# ---- Python daemon ----

install:
    pip install -e ".[dev]"

pytest *ARGS:
    python -m pytest {{ARGS}}

coverage:
    python -m pytest --cov=src/keystone --cov-report=term-missing --cov-fail-under=80

typecheck:
    mypy src/

daemon *ARGS:
    python -m keystone {{ARGS}}

# ---- Quality ----

lint:
    make lint.native

format:
    make format.native

format-check:
    make format.check.native

pre-commit:
    pre-commit run --all-files
