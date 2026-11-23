# FreeShip 26 v0.1.21 build inputs

The repository tree contains the frozen base used by the Windows and Linux packages. Both macOS DMGs use that base with the platform overlay in `.github/patches/fs26-macos-updater.patch` and the three Apple Clang compatibility edits listed below. The accompanying GitHub workflow uses the frozen snapshot as input and records these edits in its package verification output.

[macOS build and verification](https://github.com/jiangshirong/FreeShip-26/actions/runs/45201186551)

- `FreeShip-Qt/src/render/hull_view.cpp`: `if(!std::isfinite(rasterRatio)||rasterRatio<=0)return {};` → `if(!std::isfinite(rasterRatio)||rasterRatio<=0)return QPicture{};`.
- `FreeShip-Qt/src/app/startup_workflow.cpp`: `fs::PointMove{p["point_id"].toInt(),` → `fs::PointMove{static_cast<fs::EntityId>(p["point_id"].toInt()),`.
- `FreeShip-Tests/qt/fbm_codec_tests.cpp`: `"paths",QJsonArray{QJsonArray{QJsonArray{0.1,2,3},QJsonArray{4,5,6}}}` → `"paths",QJsonArray{QJsonValue(QJsonArray{QJsonArray{0.1,2,3},QJsonArray{4,5,6}})}`.
