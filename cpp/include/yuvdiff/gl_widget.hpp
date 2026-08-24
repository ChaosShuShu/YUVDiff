#pragma once

#include "yuvdiff/formats.hpp"
#include "yuvdiff/parser.hpp"
#include "yuvdiff/renderer.hpp"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

#include <memory>
#include <mutex>

namespace yuvdiff {

struct PixelInfo {
    int x = -1;
    int y = -1;
    bool has_a = false;
    int y_a = 0, u_a = 0, v_a = 0;
    bool has_b = false;
    int y_b = 0, u_b = 0, v_b = 0;
    int diff_y = 0;
};

class YUVGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit YUVGLWidget(QWidget* parent = nullptr);
    ~YUVGLWidget() override;

    // Thread-safe update of current display frames and mode
    void set_frames(
        std::shared_ptr<YUVFrame> frame_a,
        std::shared_ptr<YUVFrame> frame_b,
        RenderMode mode,
        int threshold
    );

    void clear_frames();
    void reset_zoom_pan();

signals:
    void pixelHovered(const yuvdiff::PixelInfo& info);
    void pixelLeave();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent* event) override;

    // Pan & Zoom Events
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void init_textures();
    void update_texture_plane(
        GLuint tex_id,
        int& curr_w,
        int& curr_h,
        int& curr_depth,
        int new_w,
        int new_h,
        int depth,
        const void* data
    );

    bool screen_to_pixel(const QPointF& screen_pos, int& out_x, int& out_y) const;
    void render_pixel_grid_and_values(
        QPainter& painter,
        int video_w,
        int video_h,
        float scale_x,
        float scale_y,
        const std::shared_ptr<YUVFrame>& fa,
        const std::shared_ptr<YUVFrame>& fb,
        RenderMode mode
    );

    std::unique_ptr<QOpenGLShaderProgram> shader_program_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_;

    // Pan & Zoom State
    float zoom_level_ = 1.0f;
    QPointF pan_offset_{0.0f, 0.0f};
    bool is_dragging_ = false;
    QPoint last_mouse_pos_;

    // Texture IDs
    GLuint tex_y_a_ = 0, tex_u_a_ = 0, tex_v_a_ = 0;
    GLuint tex_y_b_ = 0, tex_u_b_ = 0, tex_v_b_ = 0;

    // Track allocated texture dimensions
    int tex_w_ya_ = 0, tex_h_ya_ = 0, tex_d_ya_ = 0;
    int tex_w_ua_ = 0, tex_h_ua_ = 0, tex_d_ua_ = 0;
    int tex_w_va_ = 0, tex_h_va_ = 0, tex_d_va_ = 0;

    int tex_w_yb_ = 0, tex_h_yb_ = 0, tex_d_yb_ = 0;
    int tex_w_ub_ = 0, tex_h_ub_ = 0, tex_d_ub_ = 0;
    int tex_w_vb_ = 0, tex_h_vb_ = 0, tex_d_vb_ = 0;

    std::mutex mutex_;
    std::shared_ptr<YUVFrame> frame_a_;
    std::shared_ptr<YUVFrame> frame_b_;
    RenderMode mode_ = RenderMode::ORIGINAL_A;
    int threshold_ = 4;
    bool needs_texture_update_ = false;
};

} // namespace yuvdiff
