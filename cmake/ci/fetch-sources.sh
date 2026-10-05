#!/usr/bin/env bash
# Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#
# Fetches every SDL source tree the Linux/macOS build needs into reqs/.
#
# Why not just `git submodule update --init --recursive`? The SDL3 checkouts are
# declared in .gitmodules but are not committed as gitlinks, so git has nothing
# to update and silently succeeds with empty directories. This script reads
# .gitmodules itself and clones whatever is missing, which works both before and
# after the gitlinks land.
#
# The recorded branch is honoured verbatim (e.g. `branch = release-3.4.14`), so
# bumping .gitmodules is enough to bump the dependency.
#
# emsdk (Web builds only) is skipped unless ICG_CI_FETCH_EMSDK=1 — the desktop
# CI jobs never need it and it is a large checkout.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

if [ ! -f .gitmodules ]; then
    echo "fetch-sources: .gitmodules not found in ${repo_root}" >&2
    exit 1
fi

# `git config -f .gitmodules` reads the file without needing a repository, so
# this also works on a plain source tree that has no .git directory.
while read -r _key path; do
    name="${_key#submodule.}"
    name="${name%.path}"

    case "${path}" in
        emsdk*)
            if [ "${ICG_CI_FETCH_EMSDK:-0}" != "1" ]; then
                echo "fetch-sources: skipping ${path} (set ICG_CI_FETCH_EMSDK=1 to include it)"
                continue
            fi
            ;;
    esac

    url="$(git config -f .gitmodules --get "submodule.${name}.url")"
    branch="$(git config -f .gitmodules --get "submodule.${name}.branch" || true)"

    if [ -z "${url}" ]; then
        echo "fetch-sources: no url for submodule ${name}" >&2
        exit 1
    fi

    if [ -d "${path}/.git" ] || [ -f "${path}/CMakeLists.txt" ]; then
        echo "fetch-sources: ${path} already present — refreshing vendored submodules only"
    else
        echo "fetch-sources: cloning ${url} (${branch:-default branch}) -> ${path}"
        mkdir -p "$(dirname "${path}")"
        if [ -n "${branch}" ]; then
            git clone --depth 1 --branch "${branch}" "${url}" "${path}"
        else
            git clone --depth 1 "${url}" "${path}"
        fi
    fi

    # The SDL addons vendor freetype/harfbuzz/plutosvg, libpng/jpeg/webp/...,
    # ogg/vorbis/flac/opus/... as their own submodules. Those are what makes the
    # desktop builds need no system dev packages, so they must be initialised.
    echo "fetch-sources: initialising vendored submodules in ${path}"
    git -C "${path}" submodule update --init --recursive
done < <(git config -f .gitmodules --get-regexp '^submodule\..*\.path$')

echo "fetch-sources: done"
