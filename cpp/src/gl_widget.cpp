#include "yuvdiff/gl_widget.hpp"

#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

namespace yuvdiff {

static const char* VERTEX_SHADER_SRC = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform vec2 uScale;
uniform vec2 uPan;
uniform float uZoom;
void main() {
    vec2 pos = aPos * uScale * uZoom + uPan;
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char* FRAGMENT_SHADER_SRC = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D tex_y_a;
uniform sampler2D tex_u_a;
uniform sampler2D tex_v_a;

uniform sampler2D tex_y_b;
uniform sampler2D tex_u_b;
uniform sampler2D tex_v_b;

uniform int u_has_b;
uniform int u_mode; // 0: ORIGINAL_A, 1: ORIGINAL_B, 2: HEATMAP, 3: THRESHOLD_MASK, 4: SIDE_BY_SIDE, 5: COMPARISON
uniform float u_threshold;
uniform float u_scale_a;
uniform float u_scale_b;
uniform float u_wipe_pos; // [0.0, 1.0] split position

vec3 yuv_to_rgb(float y, float u, float v) {
    float uc = u - 0.5;
    float vc = v - 0.5;
    float r = y + 1.402 * vc;
    float g = y - 0.344136 * uc - 0.714136 * vc;
    float b = y + 1.772 * uc;
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

vec3 turbo_colormap(float x) {
    if (x <= 0.0001) return vec3(0.06, 0.06, 0.08); // 0 diff is dark
    // Non-linear amplification so small errors (1~10 levels out of 255) are vividly visible
    float t = clamp(pow(x * 6.0, 0.65), 0.0, 1.0);
    float r = clamp(1.5 - abs(t - 0.75) * 4.0, 0.0, 1.0);
    float g = clamp(1.5 - abs(t - 0.50) * 4.0, 0.0, 1.0);
    float b = clamp(1.5 - abs(t - 0.25) * 4.0, 0.0, 1.0);
    if (t > 0.85) {
        r = 1.0;
        g = (t - 0.85) / 0.15;
        b = (t - 0.85) / 0.15;
    }
    return vec3(r, g, b);
}

void main() {
    float ya = texture(tex_y_a, TexCoord).r * u_scale_a;
    float ua = texture(tex_u_a, TexCoord).r * u_scale_a;
    float va = texture(tex_v_a, TexCoord).r * u_scale_a;

    if (u_mode == 0 || u_has_b == 0) {
        FragColor = vec4(yuv_to_rgb(ya, ua, va), 1.0);
        return;
    }

    float yb = texture(tex_y_b, TexCoord).r * u_scale_b;
    float ub = texture(tex_u_b, TexCoord).r * u_scale_b;
    float vb = texture(tex_v_b, TexCoord).r * u_scale_b;

    if (u_mode == 1) {
        FragColor = vec4(yuv_to_rgb(yb, ub, vb), 1.0);
        return;
    }

    // 5: COMPARISON (Wipe line split: Left Video A, Right Video B)
    if (u_mode == 5) {
        if (TexCoord.x <= u_wipe_pos) {
            FragColor = vec4(yuv_to_rgb(ya, ua, va), 1.0);
        } else {
            FragColor = vec4(yuv_to_rgb(yb, ub, vb), 1.0);
        }
        return;
    }

    float dy = abs(ya - yb);
    float du = abs(ua - ub);
    float dv = abs(va - vb);
    float max_diff = max(dy, max(du, dv));

    if (u_mode == 2) {
        // High-contrast false-color Heatmap (Turbo/Jet palette with amplification)
        FragColor = vec4(turbo_colormap(max_diff), 1.0);
        return;
    }

    if (u_mode == 3) {
        // THRESHOLD_MASK (covers Y, U, V channels)
        vec3 rgb_a = yuv_to_rgb(ya, ua, va);
        if (max_diff > u_threshold) {
            FragColor = vec4(mix(rgb_a, vec3(1.0, 0.1, 0.1), 0.65), 1.0);
        } else {
            FragColor = vec4(rgb_a, 1.0);
        }
        return;
    }

    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)";

YUVGLWidget::YUVGLWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setMinimumSize(640, 360);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

YUVGLWidget::~YUVGLWidget() {
    makeCurrent();
    vao_.destroy();
    vbo_.destroy();

    GLuint texs[6] = {tex_y_a_, tex_u_a_, tex_v_a_, tex_y_b_, tex_u_b_, tex_v_b_};
    glDeleteTextures(6, texs);
    doneCurrent();
}

void YUVGLWidget::set_frames(
    std::shared_ptr<YUVFrame> frame_a,
    std::shared_ptr<YUVFrame> frame_b,
    RenderMode mode,
    int threshold
) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_a_ = frame_a;
        frame_b_ = frame_b;
        mode_ = mode;
        threshold_ = threshold;
        needs_texture_update_ = true;
    }
    update(); // Schedule OpenGL paint
}

void YUVGLWidget::clear_frames() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_a_.reset();
        frame_b_.reset();
        needs_texture_update_ = false;
    }
    reset_zoom_pan();
    update();
}

void YUVGLWidget::reset_zoom_pan() {
    zoom_level_ = 1.0f;
    pan_offset_ = QPointF(0.0f, 0.0f);
    update();
}

void YUVGLWidget::wheelEvent(QWheelEvent* event) {
    float old_zoom = zoom_level_;
    float factor = (event->angleDelta().y() > 0) ? 1.15f : (1.0f / 1.15f);
    zoom_level_ = std::clamp(zoom_level_ * factor, 0.1f, 80.0f);

    bool is_sbs = (mode_ == RenderMode::SIDE_BY_SIDE);
    float vp_w = is_sbs ? (width() / 2.0f) : static_cast<float>(width());
    float vp_h = static_cast<float>(height());
    float sx = static_cast<float>(event->position().x());
    if (is_sbs && sx >= vp_w) {
        sx -= vp_w;
    }

    float mx = 2.0f * sx / vp_w - 1.0f;
    float my = 1.0f - 2.0f * static_cast<float>(event->position().y()) / vp_h;

    float ratio = zoom_level_ / old_zoom;
    pan_offset_.setX(mx - (mx - pan_offset_.x()) * ratio);
    pan_offset_.setY(my - (my - pan_offset_.y()) * ratio);

    update();
    event->accept();
}

void YUVGLWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && mode_ == RenderMode::COMPARISON) {
        std::shared_ptr<YUVFrame> fa, fb;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            fa = frame_a_;
            fb = frame_b_;
        }
        if (fa || fb) {
            auto prim = fa ? fa : fb;
            int video_w = prim->width;
            int video_h = prim->height;
            float widget_ratio = static_cast<float>(width()) / std::max(1, height());
            float video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
            float scale_x = (widget_ratio > video_ratio) ? (video_ratio / widget_ratio) : 1.0f;

            float aPos_x = wipe_pos_ * 2.0f - 1.0f;
            float ndc_x = aPos_x * scale_x * zoom_level_ + pan_offset_.x();
            float wipe_sx = (ndc_x + 1.0f) * 0.5f * width();

            float mouse_x = static_cast<float>(event->position().x());
            if (std::abs(mouse_x - wipe_sx) <= 14.0f) {
                is_dragging_wipe_ = true;
                last_mouse_pos_ = event->pos();
                setCursor(Qt::SplitHCursor);
                event->accept();
                return;
            }
        }
    }

    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        is_dragging_ = true;
        last_mouse_pos_ = event->pos();
        event->accept();
    }
}

bool YUVGLWidget::screen_to_pixel(const QPointF& screen_pos, int& out_x, int& out_y) const {
    std::shared_ptr<YUVFrame> fa;
    std::shared_ptr<YUVFrame> fb;
    RenderMode mode;
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        fa = frame_a_;
        fb = frame_b_;
        mode = mode_;
    }
    if (!fa && !fb) return false;

    auto prim = fa ? fa : fb;
    int video_w = prim->width;
    int video_h = prim->height;

    bool is_sbs = (mode == RenderMode::SIDE_BY_SIDE);
    float vp_w = is_sbs ? (width() / 2.0f) : static_cast<float>(width());
    float vp_h = static_cast<float>(height());
    float sx = static_cast<float>(screen_pos.x());
    if (is_sbs && sx >= vp_w) {
        sx -= vp_w;
    }

    float widget_ratio = vp_w / std::max(1.0f, vp_h);
    float video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
    float scale_x = (widget_ratio > video_ratio) ? (video_ratio / widget_ratio) : 1.0f;
    float scale_y = (widget_ratio > video_ratio) ? 1.0f : (widget_ratio / video_ratio);

    float ndc_x = 2.0f * sx / vp_w - 1.0f;
    float ndc_y = 1.0f - 2.0f * static_cast<float>(screen_pos.y()) / vp_h;

    float quad_x = (ndc_x - pan_offset_.x()) / (scale_x * zoom_level_);
    float quad_y = (ndc_y - pan_offset_.y()) / (scale_y * zoom_level_);

    if (quad_x < -1.0f || quad_x > 1.0f || quad_y < -1.0f || quad_y > 1.0f) {
        return false;
    }

    float u = (quad_x + 1.0f) * 0.5f;
    float v = (1.0f - quad_y) * 0.5f;

    out_x = std::clamp(static_cast<int>(u * video_w), 0, video_w - 1);
    out_y = std::clamp(static_cast<int>(v * video_h), 0, video_h - 1);
    return true;
}

void YUVGLWidget::mouseMoveEvent(QMouseEvent* event) {
    std::shared_ptr<YUVFrame> fa, fb;
    RenderMode mode;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fa = frame_a_;
        fb = frame_b_;
        mode = mode_;
    }

    if (is_dragging_wipe_ && mode == RenderMode::COMPARISON && (fa || fb)) {
        auto prim = fa ? fa : fb;
        int video_w = prim->width;
        int video_h = prim->height;
        float widget_ratio = static_cast<float>(width()) / std::max(1, height());
        float video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
        float scale_x = (widget_ratio > video_ratio) ? (video_ratio / widget_ratio) : 1.0f;

        float sx = static_cast<float>(event->position().x());
        float ndc_x = (sx / width()) * 2.0f - 1.0f;
        float aPos_x = (ndc_x - pan_offset_.x()) / (scale_x * zoom_level_);
        float u = (aPos_x + 1.0f) * 0.5f;
        wipe_pos_ = std::clamp(u, 0.0f, 1.0f);
        setCursor(Qt::SplitHCursor);
        update();
        event->accept();
        return;
    }

    if (is_dragging_) {
        QPoint delta = event->pos() - last_mouse_pos_;
        last_mouse_pos_ = event->pos();

        bool is_sbs = (mode == RenderMode::SIDE_BY_SIDE);
        float vp_w = is_sbs ? (width() / 2.0f) : static_cast<float>(width());
        float dx = 2.0f * static_cast<float>(delta.x()) / vp_w;
        float dy = -2.0f * static_cast<float>(delta.y()) / height();
        pan_offset_ += QPointF(dx, dy);

        update();
        event->accept();
    }

    if (mode == RenderMode::COMPARISON && (fa || fb)) {
        auto prim = fa ? fa : fb;
        int video_w = prim->width;
        int video_h = prim->height;
        float widget_ratio = static_cast<float>(width()) / std::max(1, height());
        float video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
        float scale_x = (widget_ratio > video_ratio) ? (video_ratio / widget_ratio) : 1.0f;

        float aPos_x = wipe_pos_ * 2.0f - 1.0f;
        float ndc_x = aPos_x * scale_x * zoom_level_ + pan_offset_.x();
        float wipe_sx = (ndc_x + 1.0f) * 0.5f * width();

        float mouse_x = static_cast<float>(event->position().x());
        if (std::abs(mouse_x - wipe_sx) <= 10.0f) {
            setCursor(Qt::SplitHCursor);
        } else if (!is_dragging_ && cursor().shape() == Qt::SplitHCursor) {
            unsetCursor();
        }
    } else if (!is_dragging_ && cursor().shape() == Qt::SplitHCursor) {
        unsetCursor();
    }

    // Inspect pixel under cursor
    int px = -1, py = -1;
    if (screen_to_pixel(event->position(), px, py)) {
        PixelInfo info;
        info.x = px;
        info.y = py;

        if (fa && px >= 0 && px < fa->width && py >= 0 && py < fa->height) {
            auto [h_sub, v_sub] = chroma_subsampling(fa->pixel_format);
            info.has_a = true;
            info.y_a = fa->get_y(py, px);
            info.u_a = fa->get_u(py / v_sub, px / h_sub);
            info.v_a = fa->get_v(py / v_sub, px / h_sub);
        }

        if (fb && px >= 0 && px < fb->width && py >= 0 && py < fb->height) {
            auto [h_sub, v_sub] = chroma_subsampling(fb->pixel_format);
            info.has_b = true;
            info.y_b = fb->get_y(py, px);
            info.u_b = fb->get_u(py / v_sub, px / h_sub);
            info.v_b = fb->get_v(py / v_sub, px / h_sub);
        }

        if (info.has_a && info.has_b) {
            info.diff_y = std::abs(info.y_a - info.y_b);
            info.diff_u = std::abs(info.u_a - info.u_b);
            info.diff_v = std::abs(info.v_a - info.v_b);
            info.diff_max = std::max({info.diff_y, info.diff_u, info.diff_v});
        }

        emit pixelHovered(info);
    } else {
        emit pixelLeave();
    }
}

void YUVGLWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        is_dragging_ = false;
        is_dragging_wipe_ = false;
        event->accept();
    }
}

void YUVGLWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (mode_ == RenderMode::COMPARISON) {
            wipe_pos_ = 0.5f;
        }
        reset_zoom_pan();
        event->accept();
    }
}

void YUVGLWidget::leaveEvent(QEvent* event) {
    emit pixelLeave();
    QOpenGLWidget::leaveEvent(event);
}

void YUVGLWidget::init_textures() {
    GLuint texs[6];
    glGenTextures(6, texs);
    tex_y_a_ = texs[0];
    tex_u_a_ = texs[1];
    tex_v_a_ = texs[2];
    tex_y_b_ = texs[3];
    tex_u_b_ = texs[4];
    tex_v_b_ = texs[5];

    for (int i = 0; i < 6; ++i) {
        glBindTexture(GL_TEXTURE_2D, texs[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void YUVGLWidget::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);

    shader_program_ = std::make_unique<QOpenGLShaderProgram>();
    shader_program_->addShaderFromSourceCode(QOpenGLShader::Vertex, VERTEX_SHADER_SRC);
    shader_program_->addShaderFromSourceCode(QOpenGLShader::Fragment, FRAGMENT_SHADER_SRC);
    shader_program_->link();

    // Quad geometry (2 Triangles with texture coords)
    static const float vertices[] = {
        // Position    // TexCoord
        -1.0f,  1.0f,  0.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,

        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
    };

    vao_.create();
    vao_.bind();

    vbo_.create();
    vbo_.bind();
    vbo_.allocate(vertices, sizeof(vertices));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    vbo_.release();
    vao_.release();

    init_textures();
}

void YUVGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void YUVGLWidget::update_texture_plane(
    GLuint tex_id,
    int& curr_w,
    int& curr_h,
    int& curr_depth,
    int new_w,
    int new_h,
    int depth,
    const void* data
) {
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLint internal_fmt = (depth == 8) ? GL_RED : GL_R16;
    GLenum type = (depth == 8) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;

    if (curr_w != new_w || curr_h != new_h || curr_depth != depth) {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt, new_w, new_h, 0, GL_RED, type, data);
        curr_w = new_w;
        curr_h = new_h;
        curr_depth = depth;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, new_w, new_h, GL_RED, type, data);
    }
}

void YUVGLWidget::render_pixel_grid_and_values(
    QPainter& painter,
    int video_w,
    int video_h,
    float scale_x,
    float scale_y,
    const std::shared_ptr<YUVFrame>& fa,
    const std::shared_ptr<YUVFrame>& fb,
    RenderMode mode,
    float vp_offset_x,
    float vp_w_param,
    float vp_offset_y,
    float vp_h_param
) {
    float vp_w = (vp_w_param > 0.0f) ? vp_w_param : static_cast<float>(width());
    float vp_h = (vp_h_param > 0.0f) ? vp_h_param : static_cast<float>(height());

    float pixel_w_screen = (vp_w / video_w) * scale_x * zoom_level_;
    float pixel_h_screen = (vp_h / video_h) * scale_y * zoom_level_;

    if (pixel_w_screen < 28.0f) {
        return;
    }

    painter.save();
    if (vp_w_param > 0.0f || vp_h_param > 0.0f) {
        painter.setClipRect(QRectF(vp_offset_x, vp_offset_y, vp_w, vp_h));
    }

    auto get_screen_x = [&](float col) -> float {
        float aPos_x = (col / video_w) * 2.0f - 1.0f;
        float ndc_x = aPos_x * scale_x * zoom_level_ + pan_offset_.x();
        return vp_offset_x + (ndc_x + 1.0f) * 0.5f * vp_w;
    };

    auto get_screen_y = [&](float row) -> float {
        float aPos_y = 1.0f - (row / video_h) * 2.0f;
        float ndc_y = aPos_y * scale_y * zoom_level_ + pan_offset_.y();
        return vp_offset_y + (1.0f - ndc_y) * 0.5f * vp_h;
    };

    auto screen_to_col = [&](float sx) -> float {
        float ndc_x = ((sx - vp_offset_x) / vp_w) * 2.0f - 1.0f;
        float aPos_x = (ndc_x - pan_offset_.x()) / (scale_x * zoom_level_);
        return (aPos_x + 1.0f) * 0.5f * video_w;
    };

    auto screen_to_row = [&](float sy) -> float {
        float ndc_y = 1.0f - ((sy - vp_offset_y) / vp_h) * 2.0f;
        float aPos_y = (ndc_y - pan_offset_.y()) / (scale_y * zoom_level_);
        return (1.0f - aPos_y) * 0.5f * video_h;
    };

    float col_left = screen_to_col(vp_offset_x);
    float col_right = screen_to_col(vp_offset_x + vp_w);
    float row_top = screen_to_row(vp_offset_y);
    float row_bottom = screen_to_row(vp_offset_y + vp_h);

    int c_min = std::clamp(static_cast<int>(std::floor(std::min(col_left, col_right))), 0, video_w - 1);
    int c_max = std::clamp(static_cast<int>(std::ceil(std::max(col_left, col_right))), 0, video_w - 1);
    int r_min = std::clamp(static_cast<int>(std::floor(std::min(row_top, row_bottom))), 0, video_h - 1);
    int r_max = std::clamp(static_cast<int>(std::ceil(std::max(row_top, row_bottom))), 0, video_h - 1);

    if (c_min > c_max || r_min > r_max) {
        painter.restore();
        return;
    }

    // 1. Draw Crisp Grid Lines
    QPen grid_pen(QColor(255, 255, 255, 80));
    grid_pen.setWidthF(1.0f);
    painter.setPen(grid_pen);

    for (int c = c_min; c <= c_max + 1; ++c) {
        float sx = get_screen_x(static_cast<float>(c));
        painter.drawLine(QPointF(sx, get_screen_y(static_cast<float>(r_min))),
                         QPointF(sx, get_screen_y(static_cast<float>(r_max + 1))));
    }

    for (int r = r_min; r <= r_max + 1; ++r) {
        float sy = get_screen_y(static_cast<float>(r));
        painter.drawLine(QPointF(get_screen_x(static_cast<float>(c_min)), sy),
                         QPointF(get_screen_x(static_cast<float>(c_max + 1)), sy));
    }

    // 2. Draw In-Pixel Values
    auto [h_sub_a, v_sub_a] = fa ? chroma_subsampling(fa->pixel_format) : std::pair{1, 1};
    auto [h_sub_b, v_sub_b] = fb ? chroma_subsampling(fb->pixel_format) : std::pair{1, 1};

    bool is_compact = (pixel_w_screen < 52.0f);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setBold(true);
    int font_pixel_size = is_compact
        ? std::clamp(static_cast<int>(pixel_w_screen / 3.0f), 8, 14)
        : std::clamp(static_cast<int>(pixel_w_screen / 5.0f), 8, 20);
    font.setPixelSize(font_pixel_size);
    painter.setFont(font);

    float wipe_col_split = wipe_pos_ * video_w;

    for (int r = r_min; r <= r_max; ++r) {
        for (int c = c_min; c <= c_max; ++c) {
            float sx = get_screen_x(static_cast<float>(c));
            float sy = get_screen_y(static_cast<float>(r));
            QRectF cell_rect(sx, sy, pixel_w_screen, pixel_h_screen);

            int ya = fa ? fa->get_y(r, c) : 0;
            int ua = fa ? fa->get_u(r / v_sub_a, c / h_sub_a) : 0;
            int va = fa ? fa->get_v(r / v_sub_a, c / h_sub_a) : 0;

            int yb = fb ? fb->get_y(r, c) : 0;
            int ub = fb ? fb->get_u(r / v_sub_b, c / h_sub_b) : 0;
            int vb = fb ? fb->get_v(r / v_sub_b, c / h_sub_b) : 0;

            int dy = std::abs(ya - yb);
            int du = std::abs(ua - ub);
            int dv = std::abs(va - vb);
            int diff = std::max({dy, du, dv});

            // Determine first diff component (Y, then U, then V)
            char diff_chan = 'Y';
            int val_a = ya, val_b = yb, d_val = dy;
            if (dy > 0) {
                diff_chan = 'Y';
                val_a = ya; val_b = yb; d_val = dy;
            } else if (du > 0) {
                diff_chan = 'U';
                val_a = ua; val_b = ub; d_val = du;
            } else if (dv > 0) {
                diff_chan = 'V';
                val_a = va; val_b = vb; d_val = dv;
            }

            auto draw_shadow_text = [&](const QRectF& rect, int align, const QString& text, const QColor& col) {
                painter.setPen(QColor(0, 0, 0, 200));
                painter.drawText(rect.translated(1, 1), align, text);
                painter.drawText(rect.translated(-1, -1), align, text);
                painter.drawText(rect.translated(1, -1), align, text);
                painter.drawText(rect.translated(-1, 1), align, text);
                painter.setPen(col);
                painter.drawText(rect, align, text);
            };

            bool show_b = (mode == RenderMode::ORIGINAL_B);
            bool show_a = (mode == RenderMode::ORIGINAL_A || !fb);
            if (mode == RenderMode::COMPARISON) {
                if (static_cast<float>(c) > wipe_col_split && fb) {
                    show_b = true;
                    show_a = false;
                } else {
                    show_a = true;
                    show_b = false;
                }
            }

            if (is_compact) {
                QString text;
                QColor col = QColor(255, 255, 255);
                if (show_b && fb) {
                    text = QString::number(yb);
                } else if (show_a) {
                    text = QString::number(ya);
                } else {
                    if (diff > 0) {
                        text = QString("%1:d%2").arg(diff_chan).arg(d_val);
                        col = QColor(255, 90, 90);
                    } else {
                        text = "d:0";
                        col = QColor(160, 255, 160);
                    }
                }
                draw_shadow_text(cell_rect, Qt::AlignCenter, text, col);
            } else {
                float line_h = cell_rect.height() / 3.0f;
                QRectF r1(cell_rect.x(), cell_rect.y(), cell_rect.width(), line_h);
                QRectF r2(cell_rect.x(), cell_rect.y() + line_h, cell_rect.width(), line_h);
                QRectF r3(cell_rect.x(), cell_rect.y() + 2.0f * line_h, cell_rect.width(), line_h);

                if (show_b && fb) {
                    draw_shadow_text(r1, Qt::AlignCenter, QString("Y:%1").arg(yb), QColor(255, 255, 255));
                    draw_shadow_text(r2, Qt::AlignCenter, QString("U:%1").arg(ub), QColor(100, 220, 255));
                    draw_shadow_text(r3, Qt::AlignCenter, QString("V:%1").arg(vb), QColor(255, 185, 95));
                } else if (show_a) {
                    draw_shadow_text(r1, Qt::AlignCenter, QString("Y:%1").arg(ya), QColor(255, 255, 255));
                    draw_shadow_text(r2, Qt::AlignCenter, QString("U:%1").arg(ua), QColor(100, 220, 255));
                    draw_shadow_text(r3, Qt::AlignCenter, QString("V:%1").arg(va), QColor(255, 185, 95));
                } else {
                    if (diff > 0) {
                        draw_shadow_text(r1, Qt::AlignCenter, QString("%1: A:%2").arg(diff_chan).arg(val_a), QColor(255, 255, 255));
                        draw_shadow_text(r2, Qt::AlignCenter, QString("B:%1").arg(val_b), QColor(160, 210, 255));
                        draw_shadow_text(r3, Qt::AlignCenter, QString("d:%1").arg(d_val), QColor(255, 80, 80));
                    } else {
                        draw_shadow_text(r1, Qt::AlignCenter, QString("Y:%1").arg(ya), QColor(255, 255, 255));
                        draw_shadow_text(r2, Qt::AlignCenter, QString("U:%1 V:%2").arg(ua).arg(va), QColor(180, 220, 255));
                        draw_shadow_text(r3, Qt::AlignCenter, QString("d:0"), QColor(140, 255, 140));
                    }
                }
            }
        }
    }

    painter.restore();
}

void YUVGLWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.beginNativePainting();

    // 1. Render OpenGL Background
    paintGL();

    painter.endNativePainting();

    // 2. Render 2D Vector Overlay
    std::shared_ptr<YUVFrame> fa;
    std::shared_ptr<YUVFrame> fb;
    RenderMode mode;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fa = frame_a_;
        fb = frame_b_;
        mode = mode_;
    }

    if (!fa && !fb) return;

    auto primary_frame = fa ? fa : fb;
    int video_w = primary_frame->width;
    int video_h = primary_frame->height;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (mode == RenderMode::SIDE_BY_SIDE) {
        float half_w = width() / 2.0f;
        float full_h = static_cast<float>(height());

        float sbs_widget_ratio = half_w / std::max(1.0f, full_h);
        float sbs_video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
        float s_x = (sbs_widget_ratio > sbs_video_ratio) ? (sbs_video_ratio / sbs_widget_ratio) : 1.0f;
        float s_y = (sbs_widget_ratio > sbs_video_ratio) ? 1.0f : (sbs_widget_ratio / sbs_video_ratio);

        // Render pixel grid in left pane (Video A)
        render_pixel_grid_and_values(painter, video_w, video_h, s_x, s_y, fa, fb, RenderMode::ORIGINAL_A, 0.0f, half_w, 0.0f, full_h);

        // Render pixel grid in right pane (Video B)
        render_pixel_grid_and_values(painter, video_w, video_h, s_x, s_y, fa, fb, RenderMode::ORIGINAL_B, half_w, half_w, 0.0f, full_h);

        // Center vertical divider
        painter.setPen(QPen(QColor(0, 0, 0, 180), 4));
        painter.drawLine(QPointF(half_w, 0), QPointF(half_w, full_h));
        painter.setPen(QPen(QColor(64, 158, 255, 200), 2, Qt::DashLine));
        painter.drawLine(QPointF(half_w, 0), QPointF(half_w, full_h));

        // Pane header labels
        auto draw_badge = [&](float cx, float cy, const QString& title, const QColor& text_col) {
            QRectF rect(cx - 55, cy - 12, 110, 24);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(15, 20, 28, 200));
            painter.drawRoundedRect(rect, 4, 4);
            painter.setPen(text_col);
            painter.setFont(QFont("sans-serif", 9, QFont::Bold));
            painter.drawText(rect, Qt::AlignCenter, title);
        };

        draw_badge(half_w * 0.5f, 22.0f, "Video A (Left)", QColor(255, 255, 255));
        draw_badge(half_w * 1.5f, 22.0f, "Video B (Right)", QColor(100, 200, 255));
    } else {
        float widget_ratio = static_cast<float>(width()) / std::max(1, height());
        float video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
        float scale_x = (widget_ratio > video_ratio) ? (video_ratio / widget_ratio) : 1.0f;
        float scale_y = (widget_ratio > video_ratio) ? 1.0f : (widget_ratio / video_ratio);

        render_pixel_grid_and_values(painter, video_w, video_h, scale_x, scale_y, fa, fb, mode);

        if (mode == RenderMode::COMPARISON && fa && fb) {
            // Draw Wipe vertical divider line
            float aPos_x = wipe_pos_ * 2.0f - 1.0f;
            float ndc_x = aPos_x * scale_x * zoom_level_ + pan_offset_.x();
            float wipe_sx = (ndc_x + 1.0f) * 0.5f * width();

            float aPos_y_top = 1.0f;
            float aPos_y_bot = -1.0f;
            float ndc_y_top = aPos_y_top * scale_y * zoom_level_ + pan_offset_.y();
            float ndc_y_bot = aPos_y_bot * scale_y * zoom_level_ + pan_offset_.y();
            float screen_y_top = (1.0f - ndc_y_top) * 0.5f * height();
            float screen_y_bot = (1.0f - ndc_y_bot) * 0.5f * height();

            float y_min = std::max(0.0f, std::min(screen_y_top, screen_y_bot));
            float y_max = std::min(static_cast<float>(height()), std::max(screen_y_top, screen_y_bot));

            if (wipe_sx >= 0.0f && wipe_sx <= width()) {
                // Outer shadow
                painter.setPen(QPen(QColor(0, 0, 0, 180), 4));
                painter.drawLine(QPointF(wipe_sx, y_min), QPointF(wipe_sx, y_max));

                // Bright cyan divider line
                painter.setPen(QPen(QColor(64, 158, 255, 240), 2, Qt::SolidLine));
                painter.drawLine(QPointF(wipe_sx, y_min), QPointF(wipe_sx, y_max));

                // Center circular handle
                float cy = (y_min + y_max) * 0.5f;
                QRectF badge_rect(wipe_sx - 38, cy - 14, 76, 28);
                painter.setPen(QPen(QColor(64, 158, 255), 1.5));
                painter.setBrush(QColor(18, 24, 36, 220));
                painter.drawRoundedRect(badge_rect, 14, 14);

                painter.setFont(QFont("sans-serif", 9, QFont::Bold));
                painter.setPen(QColor(255, 255, 255));
                painter.drawText(badge_rect, Qt::AlignCenter, "◀ A | B ▶");
            }
        }
    }

    painter.end();
}

void YUVGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    std::shared_ptr<YUVFrame> fa;
    std::shared_ptr<YUVFrame> fb;
    RenderMode mode;
    int threshold;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        fa = frame_a_;
        fb = frame_b_;
        mode = mode_;
        threshold = threshold_;
    }

    if (!fa && !fb) return;

    auto primary_frame = fa ? fa : fb;
    int video_w = primary_frame->width;
    int video_h = primary_frame->height;

    const qreal dpr = devicePixelRatioF();
    int fb_w = std::max(1, static_cast<int>(std::round(width() * dpr)));
    int fb_h = std::max(1, static_cast<int>(std::round(height() * dpr)));

    // Aspect ratio letterbox calculation
    float widget_ratio = static_cast<float>(width()) / std::max(1, height());
    float video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
    float scale_x = (widget_ratio > video_ratio) ? (video_ratio / widget_ratio) : 1.0f;
    float scale_y = (widget_ratio > video_ratio) ? 1.0f : (widget_ratio / video_ratio);

    // High Zoom: switch to GL_NEAREST for crisp square pixels
    float pixel_w_screen = (static_cast<float>(width()) / video_w) * scale_x * zoom_level_;
    GLint mag_filter = (pixel_w_screen >= 2.0f) ? GL_NEAREST : GL_LINEAR;

    // Upload Textures
    if (fa) {
        const void* y_ptr = (fa->bit_depth == 8) ? (const void*)fa->y8.data() : (const void*)fa->y16.data();
        const void* u_ptr = (fa->bit_depth == 8) ? (const void*)fa->u8.data() : (const void*)fa->u16.data();
        const void* v_ptr = (fa->bit_depth == 8) ? (const void*)fa->v8.data() : (const void*)fa->v16.data();

        update_texture_plane(tex_y_a_, tex_w_ya_, tex_h_ya_, tex_d_ya_, fa->width, fa->height, fa->bit_depth, y_ptr);
        update_texture_plane(tex_u_a_, tex_w_ua_, tex_h_ua_, tex_d_ua_, fa->chroma_width(), fa->chroma_height(), fa->bit_depth, u_ptr);
        update_texture_plane(tex_v_a_, tex_w_va_, tex_h_va_, tex_d_va_, fa->chroma_width(), fa->chroma_height(), fa->bit_depth, v_ptr);
    }

    if (fb) {
        const void* y_ptr = (fb->bit_depth == 8) ? (const void*)fb->y8.data() : (const void*)fb->y16.data();
        const void* u_ptr = (fb->bit_depth == 8) ? (const void*)fb->u8.data() : (const void*)fb->u16.data();
        const void* v_ptr = (fb->bit_depth == 8) ? (const void*)fb->v8.data() : (const void*)fb->v16.data();

        update_texture_plane(tex_y_b_, tex_w_yb_, tex_h_yb_, tex_d_yb_, fb->width, fb->height, fb->bit_depth, y_ptr);
        update_texture_plane(tex_u_b_, tex_w_ub_, tex_h_ub_, tex_d_ub_, fb->chroma_width(), fb->chroma_height(), fb->bit_depth, u_ptr);
        update_texture_plane(tex_v_b_, tex_w_vb_, tex_h_vb_, tex_d_vb_, fb->chroma_width(), fb->chroma_height(), fb->bit_depth, v_ptr);
    }

    shader_program_->bind();

    // Bind texture units 0..5 with appropriate filter
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_y_a_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    shader_program_->setUniformValue("tex_y_a", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_u_a_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    shader_program_->setUniformValue("tex_u_a", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex_v_a_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    shader_program_->setUniformValue("tex_v_a", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, tex_y_b_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    shader_program_->setUniformValue("tex_y_b", 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, tex_u_b_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    shader_program_->setUniformValue("tex_u_b", 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, tex_v_b_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    shader_program_->setUniformValue("tex_v_b", 5);

    float max_val = (primary_frame->bit_depth == 8) ? 255.0f : 1023.0f;
    shader_program_->setUniformValue("u_threshold", static_cast<float>(threshold) / max_val);
    shader_program_->setUniformValue("u_has_b", fb ? 1 : 0);
    shader_program_->setUniformValue("u_wipe_pos", wipe_pos_);

    float scale_a = (fa && fa->bit_depth != 8) ? (65535.0f / 1023.0f) : 1.0f;
    float scale_b = (fb && fb->bit_depth != 8) ? (65535.0f / 1023.0f) : 1.0f;
    shader_program_->setUniformValue("u_scale_a", scale_a);
    shader_program_->setUniformValue("u_scale_b", scale_b);

    if (mode == RenderMode::SIDE_BY_SIDE) {
        int half_fb_w = fb_w / 2;

        float sbs_widget_ratio = (static_cast<float>(width()) * 0.5f) / std::max(1, height());
        float sbs_video_ratio = static_cast<float>(video_w) / std::max(1, video_h);
        float sbs_scale_x = (sbs_widget_ratio > sbs_video_ratio) ? (sbs_video_ratio / sbs_widget_ratio) : 1.0f;
        float sbs_scale_y = (sbs_widget_ratio > sbs_video_ratio) ? 1.0f : (sbs_widget_ratio / sbs_video_ratio);

        shader_program_->setUniformValue("uScale", QVector2D(sbs_scale_x, sbs_scale_y));
        shader_program_->setUniformValue("uPan", QVector2D(static_cast<float>(pan_offset_.x()), static_cast<float>(pan_offset_.y())));
        shader_program_->setUniformValue("uZoom", zoom_level_);

        glEnable(GL_SCISSOR_TEST);

        // Viewport 1: Left (Video A)
        glViewport(0, 0, half_fb_w, fb_h);
        glScissor(0, 0, half_fb_w, fb_h);
        shader_program_->setUniformValue("u_mode", 0); // ORIGINAL_A
        vao_.bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        vao_.release();

        // Viewport 2: Right (Video B)
        glViewport(half_fb_w, 0, fb_w - half_fb_w, fb_h);
        glScissor(half_fb_w, 0, fb_w - half_fb_w, fb_h);
        shader_program_->setUniformValue("u_mode", fb ? 1 : 0); // ORIGINAL_B
        vao_.bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        vao_.release();

        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, fb_w, fb_h);
    } else {
        glViewport(0, 0, fb_w, fb_h);
        shader_program_->setUniformValue("uScale", QVector2D(scale_x, scale_y));
        shader_program_->setUniformValue("uPan", QVector2D(static_cast<float>(pan_offset_.x()), static_cast<float>(pan_offset_.y())));
        shader_program_->setUniformValue("uZoom", zoom_level_);
        shader_program_->setUniformValue("u_mode", static_cast<int>(mode));

        vao_.bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        vao_.release();
    }

    shader_program_->release();

    // Clean up all OpenGL states for QPainter
    for (int i = 5; i >= 0; --i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

} // namespace yuvdiff
