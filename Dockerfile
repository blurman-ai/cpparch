# archcheck container image, published to ghcr.io/blurman-ai/archcheck on
# release (.github/workflows/release.yml, job `docker`).
#
# Base is alpine, deliberately NOT scratch/distroless: `archcheck --diff` (the
# flagship mode) fork/execs the `git` binary at runtime (src/git/) to read
# blobs and diff revisions. A scratch image has no shell and no git, so
# --diff would fail immediately; every other mode would still work, but
# shipping a crippled default is worse than a few extra MB. The archcheck
# binary itself is the release's fully static (-static) asset, so alpine's
# musl libc never touches it — only `git` needs a libc at all.
#
# Not built from source: the release workflow builds and packages the static
# binary, extracts it, and hands it to this build as `./archcheck` next to
# this Dockerfile (the build context). To build locally:
#   cp build/static/src/archcheck .
#   docker build -t archcheck:local .
FROM alpine:3.20

ARG VERSION=dev

LABEL org.opencontainers.image.source="https://github.com/blurman-ai/archcheck" \
      org.opencontainers.image.description="Architecture rules and drift checks for C++ CI" \
      org.opencontainers.image.authors="Sergey Blekher" \
      org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.version="${VERSION}"

# safe.directory is set system-wide because --diff bind-mounts a repo owned
# by the host uid, and alpine's git refuses "dubious ownership" for any
# directory it doesn't own (CVE-2022-24765 mitigation) — without this, every
# `docker run` would need an extra `git config` step just to use --diff.
RUN apk add --no-cache git \
 && git config --system --add safe.directory /work \
 && addgroup -S archcheck && adduser -S -G archcheck archcheck

COPY archcheck /usr/local/bin/archcheck

USER archcheck
WORKDIR /work

ENTRYPOINT ["archcheck"]
