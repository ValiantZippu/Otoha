# Otoha v1.0.0 — Release Execution Checklist

Everything below requires a real machine (Windows + MSVC, Linux) and cannot
be executed inside this workspace. Steps are ordered; **do not skip the
gates.** Until every ☐ execution item passes, v1.0.0 stays unpublished.

## 1. Freeze verification (done in-workspace — keep green)

- [x] Version is `1.0.0` everywhere (CMake, installer derives it, About via BuildInfo)
- [x] No stale rc/beta/dev identifiers in release-facing files
- [x] Source audit: zero TODO/FIXME/HACK/PLACEHOLDER in Source/
- [x] Repo contains README/LICENSE/CHANGELOG/CONTRIBUTING/SECURITY/BUILDING/
      ARCHITECTURE/THIRD-PARTY-NOTICES
- [x] Secret scan clean; no keys/certificates committed
- [x] License: AGPLv3 declared — vendor full license text into LICENSE.md
      before tagging (checklist inside LICENSE.md)

## 2. Build + test on the release machine

- [ ] Windows: clean clone → `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
      → build → `ctest -C Release --output-on-failure` (ALL suites pass)
- [ ] Linux: same, plus `qa_stress`, then `sh ./scripts/package-linux.sh`

## 3. Package

- [ ] Windows installer: ISCC with `OTOHA_RELEASE_VERSION=1.0.0`
      → produces `Otoha-1.0.0-Windows-x64.exe` (unsigned unless credentials configured)
- [ ] Sign if credentials exist (`signtool sign /fd SHA256 /tr …`); otherwise record NOT SIGNED
- [ ] Generate SHA-256 checksums from the actual artifacts (`sha256sum` / `certutil`)

## 4. Verify like a user (#8/#32/#33)

- [ ] Fresh Windows VM/machine: install → launch → About shows
      `Otoha 1.0.0` → record → play → select/cut/delete/undo → Enhance A/B →
      export WAV **and** one compressed format → restart → recordings still there
- [ ] Upgrade install over a previous build: recordings/projects survive
- [ ] Uninstall: app removed; recordings/projects remain (per policy)
- [ ] Linux tarball: extract → run → record/export smoke
- [ ] Re-download artifacts from GitHub and re-verify checksums (#32)

## 5. Publish (#28–#31)

- [ ] Tag the EXACT commit the artifacts were built from:
      `git tag -a v1.0.0 <commit> -m "Otoha v1.0.0" && git push origin v1.0.0`
- [ ] `gh release create v1.0.0 --title "Otoha v1.0.0" --notes-file RELEASE_NOTES_v1.0.0.md`
      attaching: Windows exe, Linux tar.gz, checksums.txt, THIRD-PARTY-NOTICES.txt
- [ ] Post-release: set CHANGELOG date, announcement draft below

## Announcement draft (#34)

> Otoha 1.0 is released.
>
> A simple open-source recording and audio-enhancement app.
>
> Record. Edit. Enhance. Export.
>
> Available now for Windows (x64), with an experimental Linux build.
> Details and downloads: <release link>

## Gate rule

Any P0/P1 failure at any checkbox ⇒ fix ⇒ rerun this checklist from step 2.
No partial releases.
