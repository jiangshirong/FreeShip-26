/* Native lines-plan assembly, adapted from current FREE!ship Plus
 * FreeLinesplanScene.pas, FreeGometry_Functions.inc and StableEdgeLoops.pas.
 * Original FREE!ship copyright 2005 Martijn van Engeland; Plus contributions
 * by Timoshenko V.F. and Mark Malakanov. GPL-2.0-or-later; see COPYING. */
#include "scene_builder.h"
#include "surface_display.h"
#include "sections.h"
#include "section_cache.h"
#include "local_geometry.h"
#include "linesplan.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QFontMetrics>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <functional>

namespace fs::native_geometry {
namespace {
using Path = CurvePath;
float coord(Vec3 p, int axis) {
    return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
}
Vec3 add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 sub(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 scale(float a, Vec3 p) {
    return {a * p.x, a * p.y, a * p.z};
}
struct Bounds {
    Vec3 lo{}, hi{};
    bool empty = true;
    void include(Vec3 p) {
        if (empty) {
            lo = hi = p;
            empty = false;
            return;
        }
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
};
struct Cut {
    SectionInput input;
    std::vector<Spline> curves;
};
using Triangle = FillTriangle;
void triangle(std::vector<Triangle> &to, Vec3 a, Vec3 b, Vec3 c, quint32 color, bool symmetric) {
    to.push_back({a,
                  b,
                  c,
                  {(a.x + b.x + c.x) / 3.f, (a.y + b.y + c.y) / 3.f, (a.z + b.z + c.z) / 3.f},
                  color,
                  symmetric});
}
std::pair<Path, Path> clip(Vec3 a, Vec3 b, Vec3 c, float z) {
    std::array<Vec3, 3> points{a, b, c};
    std::array<float, 3> sides{a.z - z, b.z - z, c.z - z};
    Path above, below;
    if (sides[0] <= 0 && sides[1] <= 0 && sides[2] <= 0)
        return {{}, {a, b, c}};
    if (sides[0] > 0 && sides[1] > 0 && sides[2] > 0)
        return {{a, b, c}, {}};
    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        auto p = points[i], q = points[j];
        float s = sides[i], t = sides[j];
        if (s <= 0)
            below.push_back(p);
        if (s >= 0)
            above.push_back(p);
        if ((s < 0 && t > 0) || (s > 0 && t < 0)) {
            double f = -s / (t - s);
            Vec3 v{float(p.x + f * (q.x - p.x)), float(p.y + f * (q.y - p.y)), float(p.z + f * (q.z - p.z))};
            above.push_back(v);
            below.push_back(v);
        }
    }
    return {above, below};
}
void sortTriangles(std::vector<Triangle> &values, int axis) {
    // Keep the original equal-depth traversal: changing it changes overlapping fills.
    std::function<void(int, int)> sort = [&](int l, int r) {
        int i = l, j = r;
        float pivot = coord(values[(l + r) / 2].center, axis);
        do {
            while (coord(values[i].center, axis) < pivot)
                ++i;
            while (pivot < coord(values[j].center, axis))
                --j;
            if (i <= j) {
                std::swap(values[i], values[j]);
                ++i;
                --j;
            }
        } while (i <= j);
        if (l < j)
            sort(l, j);
        if (i < r)
            sort(i, r);
    };
    if (values.size() > 1)
        sort(0, int(values.size()) - 1);
}
quint32 shade(const Triangle &t, int axis) {
    auto u = sub(t.b, t.a), v = sub(t.c, t.a);
    Vec3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    float length = std::max(1e-6f, std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z));
    n = {n.x / length, n.y / length, n.z / length};
    float power = axis == 0 ? 1.f : 1.5f, amount = axis == 0 ? 1.4f : 1.1f;
    float dp = float(std::pow(double(std::abs(coord(n, axis))), double(power)));
    dp = (amount - 1.f) + amount * dp;
    dp = std::min(dp, 1.f);
    quint32 color = 0;
    for (int shift : {0, 8, 16})
        color |= quint32(std::nearbyint(dp * ((t.color >> shift) & 255))) << shift;
    return color;
}
QString pointKey(Vec3 p) {
    QString s;
    for (float f : {p.x, p.y, p.z}) {
        quint32 b;
        std::memcpy(&b, &f, 4);
        s += QString::number(b, 16).rightJustified(8, '0').toUpper();
    }
    return s;
}
// Match FPC 3.2.2's Single decimal contract: ten significant digits followed
// by fixed-decimal half-up rounding, including its documented near-half rule.
// This affects text only; no geometric coordinate is rounded before export.
QString fixedSingle(float value, int places) {
    const auto scientific = QString::number(std::abs(double(value)), 'e', 9);
    const int exponent = scientific.mid(scientific.indexOf('e') + 1).toInt();
    QString digits = scientific.left(scientific.indexOf('e'));
    digits.remove('.');
    // The RTL digit generator omits trailing zeros. Keeping padding here would
    // incorrectly activate its near-half rule (e.g. Single(35.36755), four places).
    while(digits.size()>1&&digits.endsWith('0'))digits.chop(1);
    int cut = exponent + 1 + places;
    bool up = false;
    if (cut >= 0 && cut < digits.size()) {
        up = digits[cut] >= '5';
        if (digits[cut] == '4' && cut < digits.size() - 3 && digits[digits.size() - 2] >= '8') {
            up = true;
            for (int i = cut + 1; i < digits.size() - 2; ++i)
                if (digits[i] != '9')
                    up = false;
        }
    }
    QString whole = cut > 0 ? digits.left(cut).leftJustified(cut, '0') : QString("0");
    if (up) {
        int i = int(whole.size()) - 1;
        while (i >= 0 && whole[i] == '9') {
            whole[i] = '0';
            --i;
        }
        if (i < 0)
            whole.prepend('1');
        else
            whole[i] = QChar(whole[i].unicode() + 1);
    }
    whole = whole.rightJustified(places + 1, '0');
    whole.insert(whole.size() - places, '.');
    if (value < 0 && std::any_of(whole.begin(), whole.end(), [](QChar c) { return c >= '1' && c <= '9'; }))
        whole.prepend('-');
    return whole;
}
std::vector<Path> creaseLoops(const SubdivisionLevel &mesh) {
    std::vector<std::vector<std::size_t>> at(mesh.points.size()), faces(mesh.points.size());
    for (std::size_t i = 0; i < mesh.edges.size(); ++i)
        for (auto p : mesh.edges[i].points)
            at[p].push_back(i);
    for (std::size_t i = 0; i < mesh.faces.size(); ++i)
        for (auto p : mesh.faces[i].points)
            faces[p].push_back(i);
    std::vector<QString> pointKeys;
    for (auto p : mesh.points)
        pointKeys.push_back(pointKey(p.position));
    auto edgeKey = [&](std::size_t i) {
        auto e = mesh.edges[i].points;
        auto a = pointKeys[e[0]], b = pointKeys[e[1]];
        return a < b ? a + b : b + a;
    };
    auto neighborhood = [&](std::size_t p) {
        QStringList a;
        for (auto e : at[p])
            if (mesh.edges[e].crease)
                a.append(edgeKey(e));
        a.sort();
        return a.join("\r\n") + "\r\n";
    };
    struct Entry {
        std::size_t edge;
        QString key;
    };
    std::vector<Entry> entries;
    for (std::size_t i = 0; i < mesh.edges.size(); ++i) {
        auto &e = mesh.edges[i];
        if (!e.crease || std::none_of(e.faces.begin(),e.faces.end(),[&](auto f){return mesh.layers[mesh.faces[f].layer].showInLinesplan;}))
            continue;
        auto a = neighborhood(e.points[0]), b = neighborhood(e.points[1]);
        if (a > b)
            std::swap(a, b);
        QStringList f;
        for (auto fi : e.faces) {
            QStringList p;
            for (auto pi : mesh.faces[fi].points)
                p.append(pointKeys[pi]);
            p.sort();
            f.append(p.join("\r\n") + "\r\n");
        }
        f.sort();
        entries.push_back({i, edgeKey(i) + ":" + a + "|" + b + "|" + f.join("\r\n") + "\r\n"});
    }
    std::sort(entries.begin(), entries.end(), [](auto &a, auto &b) { return a.key < b.key; });
    std::vector<int> rank(mesh.edges.size(), -1);
    for (std::size_t i = 0; i < entries.size(); ++i)
        rank[entries[i].edge] = int(i);
    auto regular = [&](std::size_t p) {
        if (mesh.points[p].type == VertexType::Corner)
            return false;
        auto nf = faces[p].size(), ne = at[p].size();
        auto triangles = std::count_if(faces[p].begin(), faces[p].end(),
                                       [&](auto f) { return mesh.faces[f].points.size() == 3; });
        if (nf == 5 && ne == 5)
            return triangles == 3;
        if (nf == 6 && ne == 6)
            return triangles == 6;
        if (nf == 4 && ne == 4)
            return true;
        return ne == 3 && std::count_if(at[p].begin(), at[p].end(),
                                        [&](auto e) { return mesh.edges[e].faces.size() == 1; }) == 2;
    };
    auto next = [&](std::size_t p, std::size_t previous) {
        int best = -1;
        if (!regular(p))
            return best;
        for (auto e : at[p]) {
            if (e == previous || rank[e] < 0)
                continue;
            bool shares = false;
            for (auto f : mesh.edges[e].faces)
                if (std::find(mesh.edges[previous].faces.begin(), mesh.edges[previous].faces.end(), f) !=
                    mesh.edges[previous].faces.end())
                    shares = true;
            if (!shares && (best < 0 || rank[e] < best))
                best = rank[e];
        }
        return best;
    };
    std::vector<bool> visited(entries.size());
    std::vector<Path> result;
    for (std::size_t seed = 0; seed < entries.size(); ++seed) {
        if (visited[seed])
            continue;
        visited[seed] = true;
        auto e = mesh.edges[entries[seed].edge].points;
        if (pointKeys[e[0]] > pointKeys[e[1]])
            std::swap(e[0], e[1]);
        std::vector<std::size_t> chain{e[0], e[1]};
        for (int direction = 0; direction < 2; ++direction) {
            auto current = e[direction];
            auto index = seed;
            while (true) {
                int n = next(current, entries[index].edge);
                if (n < 0 || visited[n])
                    break;
                visited[n] = true;
                auto ends = mesh.edges[entries[n].edge].points;
                current = ends[0] == current ? ends[1] : ends[0];
                if (direction == 0)
                    chain.insert(chain.begin(), current);
                else
                    chain.push_back(current);
                index = n;
            }
        }
        Path path;
        for (auto p : chain)
            path.push_back(mesh.points[p].position);
        result.push_back(std::move(path));
    }
    return result;
}

class Builder {
  public:
    const DocumentInput &d;
    LocalGeometry& geometry;
    const SubdivisionLevel &mesh;
    std::vector<Cut> cuts;
    LinesplanOutput out;
    Bounds hull, control;
    std::array<Vec3, 4> origins{};
    float diagonalWidth = 0, waterZ = 0;
    QStringList dxf;
    QVector<Primitive> labels;
    QFontMetrics metrics;
    float labelScale = 1;
    int fillReused = 0, fillRebuilt = 0;
    double fillPrepareMs = 0, fillDrawMs = 0;
    Builder(const DocumentInput &input, LocalGeometry& local)
        : d(input), geometry(local), mesh(local.mesh()), metrics([] {
              QFont f("Arial");
              f.setPixelSize(10);
              return f;
          }()) {}
    quint32 color(const char *name) const { return quint32(d.colors[name].toObject()["value"].toInt()); }
    int dxfColor(const char *name) const { return d.colors[name].toObject()["dxf"].toInt(); }
    Vec3 project(Vec3 p, int view, int sign = -1) const {
        auto o = origins[view];
        if (view == 0)
            return {o.x + p.x, o.y + p.z, 0};
        if (view == 3)
            return {o.x + p.x, o.y + p.y, 0};
        return {o.x + sign * p.y, o.y + p.z, 0};
    }
    void primitive(const Path &points, quint32 col, int kind = 2) {
        if (points.size() < std::size_t(kind == 1 ? 3 : 2))
            return;
        Primitive p;
        p.kind = kind;
        p.color = col;
        for (auto v : points) {
            if (!std::isfinite(v.x) || !std::isfinite(v.y))
                throw std::overflow_error("Non-finite native scene");
            p.points.append({v.x, v.y});
        }
        out.scene.primitives.append(std::move(p));
    }
    void layout() {
        for (auto &f : mesh.faces)
            if (mesh.layers[f.layer].showInLinesplan)
                for (auto p : f.points)
                    hull.include(mesh.points[p].position);
        if (hull.empty) {
            hull.include(control.lo);
            hull.include(control.hi);
        }
        for (auto &c : cuts)
            for (auto &spline : c.curves)
                for (auto p : spline.points())
                    hull.include(p);
        auto mn = hull.lo, mx = hull.hi;
        float height = mx.z - mn.z, beam = 2 * mx.y, length = mx.x - mn.x, space = float(.20 * height);
        length = std::max(length, 2 * beam + space);
        for (auto &c : cuts)
            if (c.input.kind == 4)
                for (auto &s : c.curves)
                    for (auto p : s.points())
                        diagonalWidth = std::max(diagonalWidth, diagonalDistance(p, c.input.plane));
        bool diagonals = std::any_of(cuts.begin(), cuts.end(), [](auto &c) { return c.input.kind == 4; });
        Vec3 minimum{float(mn.x - .5 * space), float(mn.z - 2 * space - height - .5 * beam - .5 * space), 0};
        if (diagonals)
            minimum.y = float(minimum.y - space - diagonalWidth);
        Vec3 maximum{float(minimum.x + length + .5 * space), float(-mn.z + height + .5 * space), 0};
        float gap = (length - 2 * beam - 3 * space) / 3;
        origins[0] = {0, -mn.z, 0};
        origins[1] = {float(minimum.x + gap + .5 * beam), origins[0].y - space - height, 0};
        origins[2] = origins[1];
        origins[2].x = float(maximum.x - gap - .5 * beam);
        origins[3] = {0, float(origins[1].y - space - .5 * beam + mn.z), 0};
        auto diff = scale(.03f, sub(mx, mn));
        minimum = sub(minimum, diff);
        maximum = add(maximum, diff);
        if (maximum.x <= minimum.x) {
            minimum.x -= .5f;
            maximum.x += .5f;
        }
        if (maximum.y <= minimum.y) {
            // An elevated flat imported mesh can invert the inherited layout
            // bounds. Recover the actual projected range, not a one-unit nudge
            // that still leaves the bounds reversed. Normal hull layout stays.
            Bounds projected;
            for(int view=0;view<4;++view)for(float x:{mn.x,mx.x})for(float y:{mn.y,mx.y})for(float z:{mn.z,mx.z})
                for(int sign:{-1,1})projected.include(project({x,y,z},view,sign));
            minimum.y=projected.lo.y-.5f;maximum.y=projected.hi.y+.5f;
        }
        out.scene.bounds = QRectF(QPointF(minimum.x, minimum.y), QPointF(maximum.x, maximum.y));
        labelScale = float(.99 * std::min(1280. / (1.02 * out.scene.bounds.width()),
                                          800. / (1.02 * out.scene.bounds.height())));
    }
    float diagonalDistance(Vec3 p, const Plane &plane) const {
        float b = float(1 / std::sqrt(2.)), c = -b;
        return std::abs(b * p.y + c * p.z - plane.coefficients()[3]);
    }
    QString dimension(float value) const {
        if (!d.imperial)
            return QString::number(qint64(std::nearbyint(1000.f * value)));
        int feet = int(value);
        float inch = std::abs(value - feet) * 12;
        return (feet == 0 && value < 0 ? "-" : "") + QString::number(feet) + "' " + fixedSingle(inch, 1) +
               "''";
    }
    void grid(int view, Vec3 a, Vec3 b, const QString &text = {}, bool center = false, quint32 textColor = 0,
              quint32 lineColor = 0xc0c0c0) {
        auto o = origins[view];
        Vec3 p{o.x + a.x, o.y + a.y, 0}, q{o.x + b.x, o.y + b.y, 0};
        primitive({p, q}, lineColor);
        if (text.isEmpty())
            return;
        auto label = [&](float x, float y, int offset) {
            Primitive l;
            l.kind = 3;
            l.color = textColor;
            l.text = text;
            l.pixels = 12;
            l.points.append({double(x) - offset / labelScale, double(y)});
            labels.append(l);
        };
        int width = metrics.horizontalAdvance(text);
        if (center && p.x == q.x) {
            label(p.x, p.y, width / 2);
            label(q.x, q.y, width / 2);
        } else {
            label(q.x, p.y, 0);
            label(p.x, q.y, p.y == q.y && p.x != q.x ? width : 0);
        }
    }
    void displayGrid() {
        auto a = hull.lo, b = hull.hi;
        float z = a.z + d.draft, s = .01f * (b.x - a.x);
        QString base = d.texts["base"].toString() + " " + dimension(a.z),
                dwl = d.texts["dwl"].toString() + " " + dimension(z), center = d.texts["center"].toString();
        grid(0, {a.x - .025f * (b.x - a.x), z, 0}, {b.x + .025f * (b.x - a.x), z, 0}, {}, false, 0, 0xffffff);
        for (int view : {1, 2}) {
            float t = .05f * (b.y - a.y);
            grid(view, {-b.y - t, z, 0}, {b.y + t, z, 0}, {}, false, 0, 0xffffff);
        }
        grid(0, {a.x - s, a.z, 0}, {b.x + s, a.z, 0}, base, false, 255);
        grid(0, {a.x - s, z, 0}, {b.x + s, z, 0}, dwl, false, 255);
        for (auto &c : cuts)
            if (c.input.kind == 3) {
                float t = -c.input.plane.coefficients()[3];
                grid(0, {a.x - s, t, 0}, {b.x + s, t, 0}, dimension(t));
            }
        s = .05f * (b.z - a.z);
        for (auto &c : cuts)
            if (c.input.kind == 1) {
                float t = -c.input.plane.coefficients()[3];
                grid(0, {t, a.z - s, 0}, {t, b.z + s, 0}, dimension(t), true);
            }
        for (int view : {1, 2}) {
            s = .05f * (b.y - a.y);
            grid(view, {-b.y - s, a.z, 0}, {b.y + s, a.z, 0}, base, false, 255);
            grid(view, {-b.y - s, z, 0}, {b.y + s, z, 0}, dwl, false, 255);
            float h = .05f * (b.z - a.z);
            grid(view, {0, a.z - h, 0}, {0, b.z + h, 0}, center, true, 255);
            for (auto &c : cuts)
                if (c.input.kind == 3) {
                    float t = -c.input.plane.coefficients()[3];
                    grid(view, {-b.y - s, t, 0}, {b.y + s, t, 0}, dimension(t));
                }
            for (auto &c : cuts)
                if (c.input.kind == 2) {
                    float t = -c.input.plane.coefficients()[3];
                    for (float x : {t, -t})
                        grid(view, {x, a.z - h, 0}, {x, b.z + h, 0}, dimension(t), true);
                }
        }
        s = .01f * (b.x - a.x);
        grid(3, {a.x - s, 0, 0}, {b.x + s, 0, 0}, center, false, 255);
        for (auto &c : cuts)
            if (c.input.kind == 2) {
                float t = -c.input.plane.coefficients()[3];
                grid(3, {a.x - s, t, 0}, {b.x + s, t, 0}, dimension(t));
            }
        s = .05f * (b.y - a.y);
        for (auto &c : cuts)
            if (c.input.kind == 1) {
                float t = -c.input.plane.coefficients()[3];
                grid(3, {t, -diagonalWidth - s, 0}, {t, b.y + s, 0}, dimension(t), true);
            }
    }
    void fill() {
        QElapsedTimer timer;timer.start();
        std::vector<Triangle> below, above;
        // Roots stay in original control-face order at every subdivision level.
        // Cache clipping and the three lighting projections, retaining the
        // original global equal-depth sorting below.
        QByteArray keyData(reinterpret_cast<const char*>(&waterZ),sizeof(waterZ));
        keyData.append(d.shadeUnderwater?'1':'0');
        keyData.append(QJsonDocument(d.colors["underwater"].toObject()).toJson(QJsonDocument::Compact));
        const auto key=QCryptographicHash::hash(keyData,QCryptographicHash::Sha256);
        auto& roots=geometry.fills;
        const bool reusable=geometry.active()&&geometry.fillKey==key&&roots.size()==d.geometry.faces.size();
        roots.resize(d.geometry.faces.size());
        std::vector<bool> rebuild(roots.size());
        for(std::size_t i=0;i<roots.size();++i){
            rebuild[i]=!reusable||geometry.changedRoots()[i];
            if(rebuild[i]){roots[i]={};++fillRebuilt;}else ++fillReused;
        }
        for(const auto& f:mesh.faces){
            if(!rebuild[f.rootControlFace])continue;
            const auto& l=mesh.layers[f.layer];if(!l.showInLinesplan)continue;
            auto& target=roots[f.rootControlFace];
            quint32 wet = d.shadeUnderwater ? quint32(d.colors["underwater"].toObject()["raw"].toInt())
                                            : quint32(l.color);
                    for (std::size_t i = 2; i < f.points.size(); ++i) {
                        auto a = mesh.points[f.points[0]].position, b = mesh.points[f.points[i - 1]].position,
                             c = mesh.points[f.points[i]].position;
                        auto parts = clip(a, b, c, waterZ);
                        for (std::size_t j = 2; j < parts.first.size(); ++j)
                            triangle(target.above, parts.first[0], parts.first[j - 1], parts.first[j], l.color,
                                     l.symmetric);
                        for (std::size_t j = 2; j < parts.second.size(); ++j)
                            triangle(target.below, parts.second[0], parts.second[j - 1], parts.second[j], wet,
                                     l.symmetric);
                    }
        }
        for(std::size_t i=0;i<roots.size();++i)if(rebuild[i])for(auto* values:{&roots[i].below,&roots[i].above})
            for(auto& t:*values)for(int axis=0;axis<3;++axis)t.shades[axis]=shade(t,axis);
        for(std::size_t layer=0;layer<mesh.layers.size();++layer)
            for(std::size_t i=0;i<roots.size();++i)if(d.geometry.faces[i].layer==layer){
                below.insert(below.end(),roots[i].below.begin(),roots[i].below.end());
                above.insert(above.end(),roots[i].above.begin(),roots[i].above.end());
            }
        geometry.fillKey=key;
        fillPrepareMs=timer.nsecsElapsed()/1e6;timer.restart();
        auto draw = [&](const std::vector<Triangle> &values, int view, bool reverse) {
            for (std::size_t i = 0; i < values.size(); ++i) {
                auto &t = values[reverse ? values.size() - 1 - i : i];
                auto col = t.shades[view == 0 ? 1 : view == 3 ? 2 : 0];
                primitive({project(t.a, view), project(t.b, view), project(t.c, view)}, col, 1);
                if ((view == 1 || view == 2) && t.symmetric)
                    primitive({project(t.a, view, 1), project(t.b, view, 1), project(t.c, view, 1)}, col, 1);
            }
        };
        for (auto *values : {&below, &above})
            sortTriangles(*values, 1);
        draw(below, 0, false);
        draw(above, 0, false);
        for (auto *values : {&below, &above})
            sortTriangles(*values, 0);
        for (int view : {1, 2}) {
            draw(below, view, view == 1);
            draw(above, view, view == 1);
        }
        for (auto *values : {&below, &above})
            sortTriangles(*values, 2);
        draw(below, 3, false);
        draw(above, 3, false);
        fillDrawMs=timer.nsecsElapsed()/1e6;
    }
    void displayCreases() {
        for (auto &e : mesh.edges)
            if (e.crease && e.controlDescendant) {
                auto a = mesh.points[e.points[0]].position, b = mesh.points[e.points[1]].position;
                if (e.reversed)
                    std::swap(a, b);
                std::set<std::size_t> roots;
                for (auto f : e.faces)
                    roots.insert(mesh.faces[f].rootControlFace);
                if (roots.empty())
                    continue;
                auto owner = *roots.begin();
                roots = {owner};
                if (!d.geometry.layers[d.geometry.faces[owner].layer].showInLinesplan)
                    continue;
                for (auto root : roots) {
                    (void)root;
                    primitive({project(a, 0), project(b, 0)}, color("crease"));
                    primitive({project(a, 3), project(b, 3)}, color("crease"));
                    for (int view : {1, 2})
                        if ((view == 1 && a.x <= d.creaseSplit && b.x < d.creaseSplit) ||
                            (view == 2 && a.x >= d.creaseSplit && b.x > d.creaseSplit))
                            for (int sign : {-1, 1})
                                primitive({project(a, view, sign), project(b, view, sign)}, color("crease"));
                }
            }
    }
    Path curvePath(const Spline &spline, int view, bool exporting, int sign = -1) {
        Spline sampled(spline.points(), spline.knuckles(),
                       exporting ? 100 : std::max<std::size_t>(2, spline.fragments()));
        Path result;
        for (auto p : samplePolyline(sampled, !exporting))
            result.push_back(project(p, view, sign));
        return result;
    }
    void displayCurves() {
        for (auto &c : cuts)
            if (c.input.kind != 4)
                for (auto &spline : c.curves) {
                    auto col = color(c.input.kind == 1   ? "station"
                                     : c.input.kind == 2 ? "buttock"
                                                         : "waterline");
                    if (c.input.kind == 1) {
                        Bounds b;
                        for (auto p : spline.points())
                            b.include(p);
                        for (int view : {1, 2})
                            if ((view == 1 && b.hi.x <= d.split) || (view == 2 && b.lo.x >= d.split))
                                for (int sign : {-1, 1})
                                    primitive(curvePath(spline, view, false, sign), col);
                    } else
                        primitive(curvePath(spline, c.input.kind == 2 ? 0 : 3, false), col);
                }
    }
    QString decimal(float value, int places) {
        auto text = fixedSingle(value, places);
        while (text.endsWith('0'))
            text.chop(1);
        if (text.size() < places || text.endsWith('.'))
            text += '0';
        return text;
    }
    void code(int id, const QString &value) {
        dxf.append(QString::number(id));
        dxf.append(value);
    }
    void lineDXF(Vec3 a, Vec3 b, int view, const QString &layer, const char *col) {
        a = project(a, view);
        b = project(b, view);
        code(0, "LINE");
        code(8, layer);
        code(62, QString::number(dxfColor(col)));
        code(10, decimal(a.x, 6));
        code(20, decimal(a.y, 6));
        code(11, decimal(b.x, 6));
        code(21, decimal(b.y, 6));
    }
    void polyDXF(const Path &path, const QString &layer, const char *col) {
        if (path.size() < 2)
            return;
        code(0, "POLYLINE");
        code(8, layer);
        code(62, QString::number(dxfColor(col)));
        code(66, "1");
        for (auto p : path) {
            code(0, "VERTEX");
            code(8, layer);
            code(10, decimal(p.x, 4));
            code(20, decimal(p.y, 4));
        }
        code(0, "SEQEND");
    }
    void exportCurves(int view, int kind, const QString &layer, const char *col) {
        for (auto &c : cuts)
            if (c.input.kind == kind) {
                float location = -c.input.plane.coefficients()[3];
                if ((view == 1 && location > d.split) || (view == 2 && location < d.split))
                    continue;
                for (auto &s : c.curves) {
                    polyDXF(curvePath(s, view, true), layer, col);
                    if (view == 1 || view == 2)
                        polyDXF(curvePath(s, view, true, 1), layer, col);
                }
            }
    }
    void exportDXF() {
        auto a = hull.lo, b = hull.hi;
        float z = a.z + d.draft, s = .025f * (b.x - a.x);
        auto base = d.texts["base"].toString(), dwl = d.texts["dwl"].toString(),
             center = d.texts["center"].toString();
        code(0, "SECTION");
        code(2, "ENTITIES");
        lineDXF({a.x - s, a.z, 0}, {b.x + s, a.z, 0}, 0, base, "grid");
        lineDXF({a.x - s, 0, z}, {b.x + s, 0, z}, 0, dwl, "red");
        for (auto &c : cuts)
            if (c.input.kind == 3) {
                float t = -c.input.plane.coefficients()[3];
                lineDXF({a.x - s, 0, t}, {b.x + s, 0, t}, 0, "wlgrid", "grid");
            }
        s = .05f * (b.z - a.z);
        for (auto &c : cuts)
            if (c.input.kind == 1) {
                float t = -c.input.plane.coefficients()[3];
                lineDXF({t, 0, a.z - s}, {t, 0, b.z + s}, 0, "stationgrid", "grid");
            }
        exportCurves(0, 2, "buttocks", "buttock");
        auto loops = creaseLoops(mesh);
        for (auto &loop : loops) {
            Path p;
            for (auto v : loop)
                p.push_back(project(v, 0));
            polyDXF(p, "Knuckle_lines", "crease");
        }
        for (int view : {1, 2}) {
            s = .025f * (2 * b.y);
            lineDXF({0, -b.y - s, 0}, {0, b.y + s, 0}, view, base, "grid");
            lineDXF({0, -b.y - s, z}, {0, b.y + s, z}, view, dwl, "red");
            for (auto &c : cuts)
                if (c.input.kind == 3) {
                    float t = -c.input.plane.coefficients()[3];
                    lineDXF({0, -b.y - s, t}, {0, b.y + s, t}, view, "wlgrid", "grid");
                }
            s = .05f * (b.z - a.z);
            lineDXF({0, 0, a.z - s}, {0, 0, b.z + s}, view, center, "red");
            for (auto &c : cuts)
                if (c.input.kind == 2) {
                    float t = -c.input.plane.coefficients()[3];
                    for (float y : {t, -t})
                        lineDXF({0, y, a.z - s}, {0, y, b.z + s}, view, "buttockgrid", "grid");
                }
            exportCurves(view, 1, "stations", "station");
            for (auto &e : mesh.edges)
                if (e.crease && std::any_of(e.faces.begin(),e.faces.end(),[&](auto f){return mesh.layers[mesh.faces[f].layer].showInLinesplan;})) {
                    auto p = mesh.points[e.points[0]].position, q = mesh.points[e.points[1]].position;
                    if (e.reversed)
                        std::swap(p, q);
                    if ((view == 1 && p.x <= d.split && q.x <= d.split) ||
                        (view == 2 && p.x >= d.split && q.x >= d.split)) {
                        lineDXF(p, q, view, "Knuckle_lines", "crease");
                        p.y = -p.y;
                        q.y = -q.y;
                        lineDXF(p, q, view, "Knuckle_lines", "crease");
                    }
                }
        }
        s = .025f * (b.x - a.x);
        lineDXF({a.x - s, 0, 0}, {b.x + s, 0, 0}, 3, center, "red");
        for (auto &c : cuts)
            if (c.input.kind == 2) {
                float t = -c.input.plane.coefficients()[3];
                lineDXF({a.x - s, t, 0}, {b.x + s, t, 0}, 3, "buttockgrid", "grid");
            }
        s = .05f * (b.y - a.y);
        for (auto &c : cuts)
            if (c.input.kind == 1) {
                float t = -c.input.plane.coefficients()[3];
                lineDXF({t, -diagonalWidth - s, 0}, {t, b.y + s, 0}, 3, "stationgrid", "grid");
            }
        exportCurves(3, 3, "waterlines", "waterline");
        diagonals(true);
        for (auto &loop : loops) {
            Path p;
            for (auto v : loop)
                p.push_back(project(v, 3));
            polyDXF(p, "Knuckle_lines", "crease");
        }
        code(0, "ENDSEC");
        code(0, "EOF");
        out.dxf = (dxf.join("\r\n") + "\r\n").toUtf8();
    }
    void diagonals(bool exporting) {
        for (auto &c : cuts)
            if (c.input.kind == 4)
                for (auto &source : c.curves) {
                    Spline s(source.points(), source.knuckles(), exporting ? 100 : 200);
                    Path path;
                    float mn = 0, mx = 0;
                    bool first = true;
                    for (auto p : samplePolyline(s, !exporting)) {
                        float distance = diagonalDistance(p, c.input.plane);
                        if (first) {
                            mn = mx = distance;
                            first = false;
                        } else {
                            mn = std::min(mn, distance);
                            mx = std::max(mx, distance);
                        }
                        path.push_back({origins[3].x + p.x, origins[3].y - std::abs(distance), 0});
                    }
                    if (exporting)
                        polyDXF(path, "Diagonals", "diagonal");
                    else
                        primitive(path, color("diagonal"));
                    auto plane = c.input.plane.coefficients();
                    if (plane[2] == 0)
                        throw std::invalid_argument("Diagonal plane has no height intercept");
                    float t = -plane[3] / plane[2];
                    Vec3 a{0, float(mn * std::sin(3.141592653589793 / 4)),
                           float(t - mn * std::sin(3.141592653589793 / 4))},
                        b{0, float(mx * std::sin(3.141592653589793 / 4)),
                          float(t - mx * std::sin(3.141592653589793 / 4))};
                    for (int view : {1, 2})
                        for (int sign : {-1, 1}) {
                            if (exporting) {
                                auto p = a, q = b;
                                p.y *= sign;
                                q.y *= sign;
                                lineDXF(p, q, view, "diagonalgrid", "grid");
                            } else
                                primitive({project(a, view, sign), project(b, view, sign)}, 0xc0c0c0);
                        }
                }
    }
    LinesplanOutput run(const QString& cacheDirectory,const QByteArray& engine) {
        QElapsedTimer timer;
        timer.start();
        // Current upper layer simplifies only rebuilt curves. The surface's
        // conditional multi-chain cleanup is what warms those spline caches.
        // Preserve this effective policy until a separately approved tolerance replaces it.
        std::unique_ptr<SectionIndex> index;
        if (!d.sections.empty())
            index = std::make_unique<SectionIndex>(d.geometry, mesh);
        const double indexMs=timer.nsecsElapsed()/1e6;
        SectionCache cache(mesh,cacheDirectory,engine);
        const double cachePrepareMs=timer.nsecsElapsed()/1e6-indexMs;
        for (auto &input : d.sections) {
            auto sections = cache.intersect(*index,input.plane);
            Cut cut{input, {}};
            for (auto &c : sections.curves) {
                Spline s(c.points, c.knuckles, c.fragments);
                if (d.simplify && sections.multiple)
                    s.simplify(2);
                cut.curves.push_back(std::move(s));
            }
            cuts.push_back(std::move(cut));
        }
        double sectionMs = timer.nsecsElapsed() / 1e6;
        cache.trim();
        // Four-view coordinates share the completed cuts, never the 2D layout.
        // Sample a copy so display density cannot change the lines-plan or DXF.
        QJsonArray viewSections;
        std::size_t viewPointCount=0;
        for (const auto &cut : cuts) {
            QJsonArray paths, plane, splines;
            for (auto coefficient : cut.input.plane.coefficients()) plane.append(double(coefficient));
            for (const auto &curve : cut.curves) {
                auto sampled = curve;
                sampled.setFragments((256u << d.geometry.precision) + 1u);
                QJsonArray points;
                auto path=samplePolyline(sampled, true);
                viewPointCount+=path.size();
                if(viewPointCount>4000000)throw std::runtime_error("Section display exceeds the supported point limit");
                for (const auto &p : path)
                    points.append(QJsonArray{double(p.x),double(p.y),double(p.z)});
                if (points.size() >= 2) {
                    paths.append(points);QJsonArray controls,knuckles;
                    for(const auto& p:curve.points())controls.append(QJsonArray{double(p.x),double(p.y),double(p.z)});
                    for(bool flag:curve.knuckles())knuckles.append(flag);
                    splines.append(QJsonObject{{"points",controls},{"knuckles",knuckles}});
                }
            }
            viewSections.append(QJsonObject{{"kind",cut.input.kind},{"index",cut.input.order},
                                           {"plane",plane},{"paths",paths},{"splines",splines}});
        }
        out.sectionDisplay = QJsonDocument(QJsonObject{{"format","FreeShipSectionDisplay"},
                                  {"version",1},{"sections",viewSections}}).toJson(QJsonDocument::Compact);
        std::set<std::size_t> used;
        for (auto &e : d.geometry.edges)
            for (auto p : e.points)
                used.insert(p);
        for (auto &f : d.geometry.faces)
            for (auto p : f.points)
                used.insert(p);
        for (auto p : used)
            control.include(d.geometry.points[p].position);
        if (control.empty) {
            control.include({});
            control.include(d.geometry.points.empty() ? Vec3{1, 1, 1} : Vec3{});
        }
        waterZ = hydrostaticKeel(d.geometry) + d.draft;
        layout();
        fill();
        displayGrid();
        displayCreases();
        displayCurves();
        diagonals(false);
        for (int pass = 0; pass < 2; ++pass)
            for (auto &l : labels) {
                bool numeric = !l.text.isEmpty() && (l.text.front().isDigit() || l.text.front() == '-');
                if (numeric == bool(pass))
                    out.scene.primitives.append(l);
            }
        exportDXF();
        QElapsedTimer displayTimer;
        displayTimer.start();
        out.surfaceDisplay = buildSurfaceDisplay(d, mesh);
        const double displayMs = displayTimer.nsecsElapsed()/1e6;
        QJsonArray originArray;
        for (auto o : origins)
            originArray.append(QJsonArray{o.x, o.y, o.z});
        out.diagnostics = {{"engine", "cpp-native"},
                           {"fill_roots_reused",fillReused},{"fill_roots_rebuilt",fillRebuilt},
                           {"fill_prepare_ms",fillPrepareMs},{"fill_sort_project_ms",fillDrawMs},
                           {"surface_display", d.surfaceDisplay},
                           {"surface_display_ms", displayMs},
                           {"origins", originArray},
                           {"section_ms", sectionMs},
                           {"section_index_ms",indexMs},
                           {"section_cache_prepare_ms",cachePrepareMs},
                           {"section_cache_timings",cache.timings},
                           {"section_cache_hits", cache.hits},
                           {"section_cache_misses", cache.misses},
                           {"generation_ms", timer.nsecsElapsed() / 1e6},
                           {"sections", int(cuts.size())},
                           {"mesh_faces", int(mesh.faces.size())},
                           {"scene_primitives", int(out.scene.primitives.size())},
                           {"dxf_bytes", out.dxf.size()},
                           {"min", QJsonArray{hull.lo.x, hull.lo.y, hull.lo.z}},
                           {"max", QJsonArray{hull.hi.x, hull.hi.y, hull.hi.z}}};
        return std::move(out);
    }
};
} // namespace
LinesplanOutput buildLinesplan(const DocumentInput &document,const QString& sectionCache,const QByteArray& engine) {
    QElapsedTimer timer;timer.start();LocalGeometry geometry(document.geometry,sectionCache,engine);Builder builder(document,geometry);
    const auto subdivisionMs=timer.nsecsElapsed()/1e6;
    auto result=builder.run(sectionCache,engine);geometry.save();
    const auto local=geometry.diagnostics();for(auto it=local.begin();it!=local.end();++it)result.diagnostics[it.key()]=it.value();
    result.diagnostics["subdivision_ms"]=subdivisionMs;return result;
}
} // namespace fs::native_geometry
