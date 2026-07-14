#!/usr/bin/env bash
#
# Tag a Seraph engine release. The engine is distributed as SOURCE: consumers
# clone the tag WITH submodules and build locally (GitHub source tarballs omit
# submodules, so we document the recurse-submodules clone rather than uploading
# an archive).
#
# Usage:  scripts/release.sh <version>      e.g.  scripts/release.sh 0.2.0
#
# This creates an annotated tag locally and prints the push + `gh release`
# commands for you to run. It does NOT push or publish anything itself.
#
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="${1:-}"
if [[ -z "$VERSION" ]]; then
    echo "usage: $0 <version>   e.g. $0 0.2.0" >&2
    exit 1
fi
TAG="v${VERSION}"

# The tag must match project(Seraph VERSION x.y.z) in CMakeLists.txt.
CURRENT="$(grep -oE 'project\(Seraph VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt \
    | grep -oE '[0-9]+\.[0-9]+\.[0-9]+$' || true)"
if [[ "$CURRENT" != "$VERSION" ]]; then
    echo "CMakeLists.txt has version '$CURRENT', but you asked to release '$VERSION'." >&2
    echo "Bump 'project(Seraph VERSION $VERSION)', commit, then re-run." >&2
    exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
    echo "Working tree is not clean — commit or stash first." >&2
    exit 1
fi

git tag -a "$TAG" -m "Seraph $TAG"
echo "Created annotated tag $TAG."
echo
echo "Next steps (run these yourself):"
echo "  git push origin $TAG"
echo
echo "  # Publish a GitHub release (needs gh + auth):"
echo "  gh release create $TAG --title \"Seraph $TAG\" --notes \\"
echo "    'Source release. Use this engine version:"
echo
echo "        git clone --recurse-submodules --branch $TAG <repo-url>"
echo "        cmake -S <repo> -B <repo>/build && cmake --build <repo>/build"
echo
echo "    Then create a game project from the editor (File > New Project) — it"
echo "    find_package(Seraph)s this build via the generated CMakePresets.json.'"
