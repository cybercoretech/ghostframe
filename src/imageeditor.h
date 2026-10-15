#pragma once

#include "edithistory.h"
#include "editrecipe.h"

#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QTimer>
#include <QUrl>

#include <vips/vips8>

class PreviewProvider;

// QML-facing editor. Holds the working recipe and its undo history, renders
// previews from an in-memory proxy on a worker thread (only the newest
// request is rendered), and exports at full resolution with a Cybermeta
// XMP sidecar. Originals are never modified.
class ImageEditor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY previewChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int displayWidth READ displayWidth NOTIFY previewChanged)
    Q_PROPERTY(int displayHeight READ displayHeight NOTIFY previewChanged)
    Q_PROPERTY(double brightness READ brightness NOTIFY adjustmentsChanged)
    Q_PROPERTY(double contrast READ contrast NOTIFY adjustmentsChanged)
    Q_PROPERTY(double saturation READ saturation NOTIFY adjustmentsChanged)
    Q_PROPERTY(bool hasCrop READ hasCrop NOTIFY cropChanged)
    Q_PROPERTY(double cropX READ cropX NOTIFY cropChanged)
    Q_PROPERTY(double cropY READ cropY NOTIFY cropChanged)
    Q_PROPERTY(double cropWidth READ cropWidth NOTIFY cropChanged)
    Q_PROPERTY(double cropHeight READ cropHeight NOTIFY cropChanged)
    Q_PROPERTY(QString sourceTitle READ sourceTitle NOTIFY sourceChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString recipeJson READ recipeJson NOTIFY recipeChanged)
    Q_PROPERTY(int lastRenderMs READ lastRenderMs NOTIFY previewChanged)
public:
    explicit ImageEditor(QObject *parent = nullptr);
    ~ImageEditor() override;

    void setPreviewProvider(PreviewProvider *provider) { m_provider = provider; }
    // Defaults to ~/Pictures/Ghostframe/Exports.
    void setExportDirectory(const QString &path) { m_exportDir = path; }

    QUrl previewUrl() const { return m_previewUrl; }
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int displayWidth() const { return m_displaySize.width(); }
    int displayHeight() const { return m_displaySize.height(); }
    double brightness() const { return m_recipe.brightness; }
    double contrast() const { return m_recipe.contrast; }
    double saturation() const { return m_recipe.saturation; }
    bool hasCrop() const { return m_recipe.hasCrop; }
    double cropX() const { return m_recipe.cropX; }
    double cropY() const { return m_recipe.cropY; }
    double cropWidth() const { return m_recipe.cropWidth; }
    double cropHeight() const { return m_recipe.cropHeight; }
    QString sourceTitle() const { return m_title; }
    bool canUndo() const { return m_history.canUndo() || m_recipe != m_history.current(); }
    bool canRedo() const { return m_recipe == m_history.current() && m_history.canRedo(); }
    QString recipeJson() const { return m_recipe.toJsonString(); }
    int lastRenderMs() const { return m_lastRenderMs; }

    // Opens a working copy downloaded by PhotoService; the editor deletes it
    // when the edit session ends.
    Q_INVOKABLE void openOriginal(const QString &path, const QString &title, const QString &hash,
                                  const QString &folder, const QString &originalName);
    // Opens a file the user owns (never deleted). Used by GHOSTFRAME_EDIT.
    void openLocalFile(const QString &path);
    // Live while a slider moves; call commitEdit() when the drag ends.
    Q_INVOKABLE void setAdjustments(double brightness, double contrast, double saturation);
    Q_INVOKABLE void commitEdit();
    Q_INVOKABLE void rotateBy(int degrees);
    Q_INVOKABLE void setCrop(double x, double y, double width, double height);
    Q_INVOKABLE void clearCrop();
    Q_INVOKABLE void resetEdits();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void save(const QString &title, const QString &caption, const QString &tags);
    Q_INVOKABLE void cancel();

signals:
    void previewChanged();
    void readyChanged();
    void busyChanged();
    void statusChanged();
    void adjustmentsChanged();
    void cropChanged();
    void sourceChanged();
    void historyChanged();
    void recipeChanged();
    void saved(const QString &path);
    void closeRequested();

private:
    // `session` ties a worker result to the image it was started for, so a
    // result that arrives after cancel() or a new openOriginal() is dropped.
    struct OpenResult { int session = 0; bool ok = false; vips::VImage proxy; QSize sourceSize; QString error; };
    struct RenderResult { int session = 0; bool ok = false; QImage image; QSize size; int milliseconds = 0; QString error; };
    struct ExportResult { int session = 0; bool ok = false; QString stagingPath, path, error; };

    void setRecipe(const EditRecipe &recipe, bool commit);
    void schedulePreview();
    void startRender();
    void setBusy(bool value);
    void setStatus(const QString &value);
    bool writeCybermeta(const QString &path, const QString &title, const QString &caption, const QString &tags) const;

    PreviewProvider *m_provider = nullptr;
    QString m_sourcePath, m_title, m_hash, m_folder, m_originalName, m_status;
    QString m_pendingTitle, m_pendingCaption, m_pendingTags;
    QString m_exportDir;
    QUrl m_previewUrl;
    QTimer m_previewTimer;
    EditRecipe m_recipe;
    EditHistory m_history;
    vips::VImage m_proxy;
    QSize m_sourceSize, m_displaySize;
    EditRecipe m_exportRecipe;
    QFutureWatcher<OpenResult> m_openWatcher;
    QFutureWatcher<RenderResult> m_renderWatcher;
    QFutureWatcher<ExportResult> m_exportWatcher;
    int m_session = 0, m_previewRevision = 0, m_lastRenderMs = 0;
    bool m_ready = false, m_busy = false, m_renderPending = false, m_ownsSource = false;
};
