# GitLab Overview

GitLab is commonly used as a source control, code review, CI/CD, artifact, and release management platform.

For embedded teams, GitLab often connects developers, build servers, Docker images, cross-compilation toolchains, and hardware test labs.

## Main Concepts

| Concept | Meaning |
|---|---|
| Repository | Stores source code and history |
| Branch | Isolated line of development |
| Merge Request | Code review and merge workflow |
| Pipeline | Automated build/test/release process |
| Job | One task inside a pipeline |
| Runner | Machine or container that executes jobs |
| Artifact | Build output kept by GitLab |
| Environment | Deployment target or logical release environment |

## Merge Request Workflow

Typical flow:

```text
create branch
  -> commit changes
  -> push branch
  -> open merge request
  -> CI pipeline runs
  -> review comments
  -> approvals
  -> merge to main/release branch
```

## GitLab CI Basics

GitLab CI is configured using `.gitlab-ci.yml`.

Simple example:

```yaml
stages:
  - build
  - test

build:
  stage: build
  image: gcc:latest
  script:
    - g++ -std=c++17 main.cc -o app

test:
  stage: test
  image: gcc:latest
  script:
    - ./app
```

## GitLab Runner

A runner executes CI jobs.

Runner types:

- Shared runner: available to many projects
- Specific runner: registered for one project/group
- Shell runner: runs directly on a machine
- Docker runner: runs each job inside a container
- Kubernetes runner: runs jobs as pods

For embedded projects, runners may be connected to:

- Cross-compilation toolchains
- Build cache storage
- Hardware devices
- Serial consoles
- Flashing tools

## Artifacts And Cache

Artifacts are build outputs that should be saved after a job.

Cache stores reusable dependencies or build outputs to speed up future jobs.

Simple rule:

- Use artifacts to pass outputs between jobs.
- Use cache to speed up repeated builds.

## GitLab In Automotive / Embedded

GitLab is often used for:

- Source control and branch management
- Code review through merge requests
- CI pipelines for build and test
- Release branches and tags
- Traceability from requirement to commit to binary
- Artifact storage for firmware/images

## Interview Notes

Good points to mention:

- Merge requests improve review quality and traceability.
- Pipelines should run automatically on merge requests.
- Runners must match the build environment.
- Docker runners improve reproducibility.
- Hardware-dependent jobs may need protected or dedicated runners.
- Release artifacts should be tied to commit SHA, tag, pipeline ID, and configuration.
