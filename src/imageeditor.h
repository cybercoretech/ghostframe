#pragma once

#include "edithistory.h"
#include "editrecipe.h"
#include "recipestore.h"

#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <vips/vips8>

class PreviewProvider;

// QML-facing editor ("Image Lab"). Holds the working recipe, its labelled
// undo history and the per-photo RecipeStore. Renders on worker threads,
// keeping only the newest request: a small draft while a slider is dragged,
// the full preview proxy once it's released, and real full-resolution pixels
// for 1:1 zoom. Each render also produces the histogram and, on request, a
// "before" frame and clipping warnings. Exports at full resolution with a
// Cybermeta XMP sidecar. Originals are never modified.
class ImageEditor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY previewChanged)
    Q_PROPERTY(QUrl beforeUrl READ beforeUrl NOTIFY previewChanged)
    Q_PROPERTY(QUrl histogramUrl READ histogramUrl NOTIFY previewChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int displayWidth READ displayWidth NOTIFY previewChanged)
    Q_PROPERTY(int displayHeight READ displayHeight NOTIFY previewChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY recipeChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY recipeChanged)
    Q_PROPERTY(QVariantMap recipe READ recipe NOTIFY recipeChanged)
    Q_PROPERTY(double brightness READ brightness NOTIFY recipeChanged)
    Q_PROPERTY(double contrast READ contrast NOTIFY recipeChanged)
    Q_PROPERTY(double saturation READ saturation NOTIFY recipeChanged)
    Q_PROPERTY(bool hasCrop READ hasCrop NOTIFY recipeChanged)
    Q_PROPERTY(double cropX READ cropX NOTIFY recipeChanged)
    Q_PROPERTY(double cropY READ cropY NOTIFY recipeChanged)
    Q_PROPERTY(double cropWidth READ cropWidth NOTIFY recipeChanged)
    Q_PROPERTY(double cropHeight READ cropHeight NOTIFY recipeChanged)
    Q_PROPERTY(QString sourceTitle READ sourceTitle NOTIFY sourceChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QStringList historyLabels READ historyLabels NOTIFY historyChanged)
    Q_PROPERTY(int historyIndex READ historyIndex NOTIFY historyChanged)
    Q_PROPERTY(QString recipeJson READ recipeJson NOTIFY recipeChanged)
    Q_PROPERTY(int lastRenderMs READ lastRenderMs NOTIFY previewChanged)
    Q_PROPERTY(bool showClipping READ showClipping WRITE setShowClipping NOTIFY viewOptionsChanged)
    Q_PROPERTY(bool showBefore READ showBefore WRITE setShowBefore NOTIFY viewOptionsChanged)
    Q_PROPERTY(bool cropMode READ cropMode WRITE setCropMode NOTIFY viewOptionsChanged)
    Q_PROPERTY(bool zoomed READ zoomed NOTIFY zoomChanged)
    Q_PROPERTY(QUrl zoomUrl READ zoomUrl NOTIFY zoomChanged)
    Q_PROPERTY(QRectF zoomRegion READ zoomRegion NOTIFY zoomChanged)
public:
    explicit ImageEditor(QObject *parent = nullptr);
    ~ImageEditor() override;

    void setPreviewProvider(PreviewProvider *provider) { m_provider = provider; }
    // Defaults to ~/Pictures/Ghostframe/Exports.
    void setExportDirectory(const QString &path) { m_exportDir = path; }
    // Defaults to <AppData>/recipes.
    void setRecipeStore(const RecipeStore &store) { m_store = store; }

    QUrl previewUrl() const { return m_previewUrl; }
    QUrl beforeUrl() const { return m_beforeUrl; }
    QUrl histogramUrl() const { return m_histogramUrl; }
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int displayWidth() const { return m_displaySize.width(); }
    int displayHeight() const { return m_displaySize.height(); }
    int frameWidth() const;
    int frameHeight() const;
    QVariantMap recipe() const { return m_recipe.toVariantMap(); }
    const EditRecipe &currentRecipe() const { return m_recipe; }
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
    QStringList historyLabels() const { return m_history.labels(); }
    int historyIndex() const { return m_history.index(); }
    QString recipeJson() const { return m_recipe.toJsonString(); }
    int lastRenderMs() const { return m_lastRenderMs; }
    bool showClipping() const { return m_showClipping; }
    void setShowClipping(bool value);
    bool showBefore() const { return m_showBefore; }
    void setShowBefore(bool value);
    // While cropping, previews show the whole uncropped frame so the crop can
    // be redrawn or moved; cropX/Y/W/H still describe the crop.
    bool cropMode() const { return m_cropMode; }
    void setCropMode(bool value);
    bool zoomed() const { return m_zoomed; }
    QUrl zoomUrl() const { return m_zoomUrl; }
    QRectF zoomRegion() const { return m_zoomRegion; }

    // Opens a working copy downloaded by PhotoService; the editor deletes it
    // when the edit session ends. A remembered edit for `hash` is restored.
    Q_INVOKABLE void openOriginal(const QString &path, const QString &title, const QString &hash,
                                  const QString &folder, const QString &originalName);
    // Opens a file the user owns (never deleted). Used by GHOSTFRAME_EDIT.
    void openLocalFile(const QString &path);

    // Live while a slider moves (draft-quality preview); call commitEdit()
    // when the drag ends.
    Q_INVOKABLE void setParameter(const QString &key, const QVariant &value);
    Q_INVOKABLE void setAdjustments(double brightness, double contrast, double saturation);
    // Records the live state as one undo step with this label.
    Q_INVOKABLE void commitEdit(const QString &label = QString());
    // setParameter + commitEdit, for buttons and toggles.
    Q_INVOKABLE void applyParameter(const QString &key, const QVariant &value, const QString &label);
    // Puts these parameters back to their defaults as one undo step.
    Q_INVOKABLE void resetParameters(const QStringList &keys, const QString &label);
    Q_INVOKABLE void rotateBy(int degrees);
    Q_INVOKABLE void flip(bool horizontal);
    Q_INVOKABLE void setCrop(double x, double y, double width, double height);
    Q_INVOKABLE void clearCrop();
    Q_INVOKABLE void resetEdits();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void jumpToHistory(int index);

    // 1:1 zoom. `centerX/Y` are 0..1 in the output frame; `viewport` is the
    // on-screen size in pixels. Panning moves the centre by screen pixels.
    Q_INVOKABLE void setZoom(bool on, double centerX, double centerY, int viewportWidth, int viewportHeight);
    Q_INVOKABLE void panZoom(double dxPixels, double dyPixels);

    // `count` evenly spaced values of a curve ("curve.master", "curve.red", …)
    // through the engine's own interpolation, for drawing the curve editor.
    Q_INVOKABLE QVariantList curveSamples(const QString &key, int count) const;

    Q_INVOKABLE void save(const QString &title, const QString &caption, const QString &tags);
    Q_INVOKABLE void cancel();

signals:
    void previewChanged();
    void readyChanged();
    void busyChanged();
    void statusChanged();
    void recipeChanged();
    void sourceChanged();
    void historyChanged();
    void viewOptionsChanged();
    void zoomChanged();
    void saved(const QString &path);
    void closeRequested();

private:
    // `session` ties a worker result to the image it was started for, so a
    // result that arrives after cancel() or a new openOriginal() is dropped.
    struct OpenResult { int session = 0; bool ok = false; vips::VImage proxy, draft; QSize sourceSize; QString error; };
    struct RenderResult {
        int session = 0; bool ok = false, draft = false;
        QImage preview, before, histogram; QSize size; int milliseconds = 0; QString error;
    };
    struct ZoomResult { int session = 0; bool ok = false; vips::VImage original; QImage image; QRectF region; QString error; };
    struct ExportResult { int session = 0; bool ok = false; QString stagingPath, path, error; };

    void setRecipe(const EditRecipe &recipe, bool commit, const QString &label = QString());
    void persist();
    void schedulePreview();
    void startRender();
    void startZoomRender();
    QSize outputSize(const EditRecipe &recipe) const;
    void setBusy(bool value);
    void setStatus(const QString &value);
    bool writeCybermeta(const QString &path, const QString &title, const QString &caption, const QString &tags) const;
    static QString labelFor(const QString &key);

    PreviewProvider *m_provider = nullptr;
    RecipeStore m_store;
    QString m_storeKey, m_lastLabel;
    QString m_sourcePath, m_title, m_hash, m_folder, m_originalName, m_status;
    QString m_pendingTitle, m_pendingCaption, m_pendingTags;
    QString m_exportDir;
    QUrl m_previewUrl, m_beforeUrl, m_histogramUrl, m_zoomUrl;
    QTimer m_previewTimer, m_zoomTimer;
    EditRecipe m_recipe, m_exportRecipe;
    EditHistory m_history;
    vips::VImage m_proxy, m_draft, m_original;
    QSize m_sourceSize, m_displaySize, m_zoomViewport;
    QPointF m_zoomCenter{0.5, 0.5};
    QRectF m_zoomRegion;
    QFutureWatcher<OpenResult> m_openWatcher;
    QFutureWatcher<RenderResult> m_renderWatcher;
    QFutureWatcher<ZoomResult> m_zoomWatcher;
    QFutureWatcher<ExportResult> m_exportWatcher;
    int m_session = 0, m_revision = 0, m_lastRenderMs = 0;
    bool m_ready = false, m_busy = false, m_renderPending = false, m_zoomPending = false, m_ownsSource = false;
    bool m_showClipping = false, m_showBefore = false, m_zoomed = false, m_cropMode = false;
};
