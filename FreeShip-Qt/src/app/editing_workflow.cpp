#include "main_window.h"
#include "project_dialog.h"
#include "layer_panel.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <algorithm>
#include <stdexcept>

namespace {
template<class T> T* editor(QWidget* field) {
    auto* value = qobject_cast<T*>(field);
    if (!value) throw std::runtime_error("Missing production property editor");
    return value;
}
QByteArray bytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read editing evidence");
    return file.readAll();
}
bool same(fs::Vec3 a, fs::Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
}

// Hidden verification uses the production widgets and command path, with all
// outputs confined to the caller's private evidence directory.
void MainWindow::verifyEditing(const QString& model, const QString& output) {
    editingWorkflow_ = true;
    verifyWorkflow(model, output);
}

void MainWindow::editingStep(const QString& method) {
    if (workflowFinished_) return;
    try {
        const auto model = session_.model();
        switch (workflowStage_++) {
        case 0: {
            if (method != "load") throw std::runtime_error("Expected editing model load");
            captureWorkflow("original"); editingOriginal_ = model;
            const auto revision = model->revision;
            ProjectDialog cancelled(*model, this);
            editor<QLineEdit>(cancelled.fieldEditor("name"))->setText("Cancelled project");
            cancelled.reject();
            workflowCase("project_cancel_unchanged", cancelled.result() == QDialog::Rejected &&
                session_.model()->revision == revision && !session_.busy() && !session_.modified());
            ProjectDialog dialog(*model, this);
            editor<QLineEdit>(dialog.fieldEditor("name"))->setText("Qt M2 project");
            editor<QLineEdit>(dialog.fieldEditor("designer"))->setText("Qt M2 designer");
            editor<QPlainTextEdit>(dialog.fieldEditor("comment"))->setPlainText("Qt M2 comment");
            editor<QDoubleSpinBox>(dialog.fieldEditor("draft"))->setValue(model->draft + 0.125);
            if (!dialog.validationError().isEmpty()) throw std::runtime_error(dialog.validationError().toUtf8().constData());
            dialog.accept();
            workflowCase("project_dialog_accepted", dialog.result() == QDialog::Accepted);
            if (!session_.updateProject(dialog.changes())) throw std::runtime_error("Project command was not submitted");
            break;
        }
        case 1: {
            if (method != "update_project") throw std::runtime_error("Expected project update");
            captureWorkflow("project"); editingChanged_ = model; workflowOriginalDigest_ = bytes(session_.scenePath());
            workflowCase("project_fields_applied", model->name == "Qt M2 project" && model->designer == "Qt M2 designer" &&
                model->comment == "Qt M2 comment" && model->draft == double(float(editingOriginal_->draft + 0.125)));
            workflowCase("project_one_revision", model->revision == editingOriginal_->revision + 1 && session_.canUndo() && session_.modified());
            ProjectDialog dialog(*model, this);
            auto* units = editor<QComboBox>(dialog.fieldEditor("units"));
            const int next = units->findData(model->units == "metric" ? "imperial" : "metric");
            if (next < 0) throw std::runtime_error("Missing alternate unit choice");
            units->setCurrentIndex(next);
            dialog.accept();
            workflowCase("units_only_patch", dialog.changes().size() == 1 && dialog.changes().contains("units"));
            if (!session_.updateProject(dialog.changes())) throw std::runtime_error("Unit command was not submitted");
            break;
        }
        case 2:
            if (method != "update_project") throw std::runtime_error("Expected unit update");
            captureWorkflow("units");
            workflowCase("units_applied", model->units != editingChanged_->units && model->revision == editingChanged_->revision + 1);
            undo_->trigger(); break;
        case 3: {
            if (method != "undo") throw std::runtime_error("Expected unit undo");
            captureWorkflow("units_undo");
            workflowCase("units_undo_exact_scene", bytes(session_.scenePath()) == workflowOriginalDigest_ && model->units == editingChanged_->units);
            if (model->layers.isEmpty()) throw std::runtime_error("Fixture has no layer");
            editingLayer_ = model->layers.first().id; layers_->setSelectedLayerId(editingLayer_);
            editor<QLineEdit>(layers_->fieldEditor("name"))->setText("Qt M2 layer");
            auto* color = layers_->fieldEditor("color");
            // The color editor exposes its stored Windows RGB value for hidden
            // verification; its ordinary button still opens the color chooser.
            color->setProperty("colorValue", 0x112233);
            editor<QDoubleSpinBox>(layers_->fieldEditor("thickness"))->setValue(12.5);
            editor<QDoubleSpinBox>(layers_->fieldEditor("material_density"))->setValue(7.5);
            if (!layers_->validationError().isEmpty()) throw std::runtime_error(layers_->validationError().toUtf8().constData());
            if (!layers_->grab().save(workflowOutput_ + "/views/layers.png")) throw std::runtime_error("Cannot save layer evidence");
            layers_->applyChanges();
            if (!session_.busy()) throw std::runtime_error("Layer apply did not submit a command");
            break;
        }
        case 4: {
            if (method != "update_layer") throw std::runtime_error("Expected layer update");
            captureWorkflow("layer");
            const auto* layer = model->layer(editingLayer_);
            workflowCase("layer_fields_applied", layer && layer->name == "Qt M2 layer" && layer->color == 0x112233 &&
                layer->thickness == 12.5 && layer->materialDensity == 7.5);
            editingChanged_ = model; workflowOriginalDigest_ = bytes(session_.scenePath());
            QSet<fs::EntityId> ids;
            for (const auto& point : model->points) {
                if (point.locked || !point.batchMovable) continue;
                views_.front()->setSelectedPoint(point.id);
                if (views_.front()->selectedPoint() != point.id) continue;
                ids.insert(point.id); editingMoves_.append({point.id, point.position});
                if (ids.size() == 2) break;
            }
            if (ids.size() != 2) throw std::runtime_error("Fixture has fewer than two visible editable points");
            selectPoints(ids); coordinates_[2]->setValue(0.01); apply_->click();
            if (!session_.busy()) throw std::runtime_error("Batch apply did not submit a command");
            break;
        }
        case 5: {
            if (method != "move_points") throw std::runtime_error("Expected batch move");
            captureWorkflow("batch"); workflowMovedDigest_ = bytes(session_.scenePath());
            bool valid = model->revision == editingChanged_->revision + 1;
            for (const auto& original : editingMoves_) {
                const auto* p = model->point(original.id);
                valid &= p && p->position.x == original.position.x && p->position.y == original.position.y &&
                    p->position.z == double(float(original.position.z + 0.01));
            }
            workflowCase("batch_one_command_coordinates", valid); undo_->trigger(); break;
        }
        case 6: {
            if (method != "undo") throw std::runtime_error("Expected batch undo");
            captureWorkflow("batch_undo"); bool valid = true;
            for (const auto& original : editingMoves_) {
                const auto* p = model->point(original.id); valid &= p && same(p->position, original.position);
            }
            workflowCase("batch_undo_exact", valid && bytes(session_.scenePath()) == workflowOriginalDigest_);
            redo_->trigger(); break;
        }
        case 7: {
            if (method != "redo") throw std::runtime_error("Expected batch redo");
            captureWorkflow("batch_redo");
            workflowCase("batch_redo_exact_scene", bytes(session_.scenePath()) == workflowMovedDigest_);
            editingChanged_ = model;
            const QString saved = workflowOutput_ + "/saved.fbm"; workflowArtifacts_.insert("saved_file", saved);
            session_.save(saved); break;
        }
        case 8:
            if (method != "save") throw std::runtime_error("Expected editing save");
            workflowCase("save_clean", !session_.modified());
            session_.load(workflowArtifacts_.value("saved_file").toString()); break;
        case 9: {
            if (method != "load") throw std::runtime_error("Expected editing reopen");
            captureWorkflow("reopened");
            workflowCase("reopen_project_fields", model->name == editingChanged_->name && model->designer == editingChanged_->designer &&
                model->comment == editingChanged_->comment && model->draft == editingChanged_->draft && model->units == editingChanged_->units);
            const auto layer = std::find_if(model->layers.begin(), model->layers.end(), [](const auto& item) { return item.name == "Qt M2 layer"; });
            workflowCase("reopen_layer_material_fields", layer != model->layers.end() && layer->color == 0x112233 &&
                layer->thickness == 12.5 && layer->materialDensity == 7.5);
            workflowCase("reopen_exact_scene", bytes(session_.scenePath()) == workflowMovedDigest_);
            workflowCase("reopen_clean", !session_.modified() && !session_.canUndo() && !session_.canRedo());
            workflowCases_.append(QJsonObject{{"name", "desktop_acceptance"}, {"status", "not_run"},
                {"message", "Hidden production widgets only; new editing gestures need user desktop acceptance."}});
            finishWorkflow(); break;
        }
        default: throw std::runtime_error("Unexpected editing callback");
        }
    } catch (const std::exception& ex) { finishWorkflow(QString::fromUtf8(ex.what())); }
}
