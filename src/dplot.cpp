#include "dplot.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>
#include <matplot/matplot.h>

namespace dplot {
    namespace fs = std::filesystem;

    const std::string Environment::PATH_PDFLATEX = "pdflatex";
    const std::string Environment::PATH_PDF2SVG = "pdf2svg";
    const std::string Environment::PATH_SCOUR = "scour";

    // --- Anonymous namespace for internal helpers and LatexOutput ---
    namespace {
        std::string fmt_flt(double x) {
            std::ostringstream oss;
            oss << std::scientific << std::setprecision(20) << x;
            return oss.str();
        }

        std::string bool_to_str(bool b) { return b ? "true" : "false"; }

        char get_axis_kind(char val) {
            if (val == 't' || val == 'b') return 'x';
            if (val == 'l' || val == 'r') return 'y';
            return '\0';
        }

        char get_opposite_axis_kind(char axis_kind) {
            if (axis_kind == 'y') return 'x';
            if (axis_kind == 'x') return 'y';
            return '\0';
        }

        std::string get_axis_pos(char axis) {
            if (axis == 't') return "top";
            if (axis == 'l') return "left";
            if (axis == 'r') return "right";
            if (axis == 'b') return "bottom";
            return "";
        }

        void append(std::vector<std::string> &out, const std::vector<std::string> &in) {
            out.insert(out.end(), in.begin(), in.end());
        }

        // --- LatexOutput Class ---
        class LatexOutput {
        private:
            Figure *fig;
            double overscale_limit = 1e10;

            const std::vector<std::string> LatexCmdsDocClass = {R"(\documentclass[class=IEEEtran]{standalone})"};
            const std::vector<std::string> LatexCmdsAfterDocClass = {
                R"(\usepackage{tikz,amsmath,siunitx})",
                R"(\sisetup{range-units=repeat, list-units=repeat, binary-units, exponent-product = \cdot, print-unity-mantissa=false})",
                R"(\usetikzlibrary{arrows,snakes,backgrounds,patterns,matrix,shapes,fit,calc,shadows,plotmarks})",
                R"(\usepackage[graphics,tightpage,active]{preview})",
                R"(\usepackage{pgfplots})",
                R"(\pgfplotsset{compat=newest})",
                R"(\usetikzlibrary{shapes.geometric})",
                R"(\PreviewEnvironment{tikzpicture})",
                R"(\PreviewEnvironment{equation})",
                R"(\PreviewEnvironment{equation*})",
                R"(\newlength\figurewidth)",
                R"(\newlength\figureheight)"
            };

            [[nodiscard]] std::optional<std::pair<double, double> > get_y_domain(const AxisSetup &asy) const {
                if (!asy.limits.has_value() || asy.log) return std::nullopt;
                double mn = asy.limits->first > 0
                                ? asy.limits->first / overscale_limit
                                : asy.limits->first * overscale_limit;
                double mx = asy.limits->second > 0
                                ? asy.limits->second * overscale_limit
                                : asy.limits->second / overscale_limit;
                return std::make_pair(mn, mx);
            }

            [[nodiscard]] std::vector<std::string> get_axis_param(char axis_kind,
                                                                  const std::optional<AxisSetup> &axis_setup,
                                                                  std::optional<std::pair<double, double> > limits =
                                                                          std::nullopt) const {
                if (!limits.has_value() && axis_setup.has_value()) { limits = axis_setup->limits; }
                return {
                    "scale only axis", "width=" + fig->width, "height=" + fig->height,
                    std::string(1, axis_kind) + "min=" + fmt_flt(limits->first),
                    std::string(1, axis_kind) + "max=" + fmt_flt(limits->second)
                };
            }

            [[nodiscard]] std::vector<std::string> create_doc_begin() const {
                std::vector<std::string> out = {
                    "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%", "% auto-generated using dplot %", "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
                };
                append(out, LatexCmdsDocClass);
                append(out, LatexCmdsAfterDocClass);
                out.emplace_back(R"(\begin{document})");
                out.push_back(R"(\setlength\figurewidth{)" + fig->width + R"(})");
                out.push_back(R"(\setlength\figureheight{)" + fig->height + R"(})");
                out.emplace_back(R"(\begin{tikzpicture}[font=\normalsize])");
                out.push_back(
                    R"(\pgfplotsset{every axis/.append style={)" + fig->basic_thickness + R"(},compat=1.18},)");
                return out;
            }

            [[nodiscard]] std::vector<std::string> create_padding() const {
                std::vector<std::string> out = {"", "%%%%%%%%%%%", "% padding %", "%%%%%%%%%%%"};
                for (const auto &[axis, axis_setup]: fig->axes) {
                    if (!axis_setup.has_value()) continue;
                    char axis_kind = get_axis_kind(axis);
                    char axis_kind_op = get_opposite_axis_kind(axis_kind);
                    auto params = get_axis_param(axis_kind, axis_setup, std::make_pair(0.0, 1.0));

                    std::string label_str = (axis_kind == 'y') ? R"({\hphantom{-}})" : R"({\vphantom{-}})";
                    params.push_back(std::string(1, axis_kind) + "mode=linear");
                    params.push_back("log basis " + std::string(1, axis_kind) + "=" + axis_setup->log_base);
                    params.push_back(std::string(1, axis_kind_op) + "min=0");
                    params.push_back(std::string(1, axis_kind_op) + "max=1");
                    params.emplace_back(R"(xtick=\empty)");
                    params.emplace_back(R"(ytick=\empty)");
                    params.push_back("hide " + std::string(1, axis_kind_op) + " axis=true");
                    params.push_back(std::string(1, axis_kind) + "tick style={draw=none}");
                    params.push_back(std::string(1, axis_kind) + "label=" + label_str);
                    params.push_back(std::string(1, axis_kind) + "label shift=" + axis_setup->padding);
                    params.push_back(std::string(1, axis_kind) + "ticklabel pos=" + get_axis_pos(axis));

                    out.push_back(R"(\begin{axis}% )" + std::string(1, axis) + "-axis");
                    out.emplace_back("[");
                    for (const auto &p: params) out.push_back("  " + p + ",");
                    out.emplace_back("]");
                    out.emplace_back(R"(\end{axis})");
                }
                return out;
            }

            [[nodiscard]] std::vector<std::string> create_background() const {
                std::vector<std::string> out = {"", "%%%%%%%%%%%%%%", "% background %", "%%%%%%%%%%%%%%"};
                bool background_color_applied = false;

                for (const auto &[axis, axis_setup_opt]: fig->axes) {
                    if (!axis_setup_opt.has_value()) continue;
                    const auto &axis_setup = axis_setup_opt.value();
                    const char axis_kind = get_axis_kind(axis);
                    const char axis_kind_op = get_opposite_axis_kind(axis_kind);
                    auto params = get_axis_param(axis_kind, axis_setup);

                    if (!background_color_applied) {
                        params.push_back("axis background/.style={fill=" + fig->background_color + "}");
                        background_color_applied = true;
                    }

                    params.push_back(std::string(1, axis_kind) + "mode=" + (axis_setup.log ? "log" : "linear"));
                    params.push_back("log basis " + std::string(1, axis_kind) + "=" + axis_setup.log_base);
                    params.push_back(std::string(1, axis_kind_op) + "min=0");
                    params.push_back(std::string(1, axis_kind_op) + "max=1");
                    params.push_back(std::string(1, axis_kind) + "label={" + axis_setup.label + "}");
                    params.push_back(std::string(1, axis_kind) + "label shift={" + axis_setup.label_shift + "}");
                    params.emplace_back(R"(xticklabel=\empty)");
                    params.emplace_back(R"(yticklabel=\empty)");
                    params.push_back(
                        std::string(1, axis_kind) + "majorgrids=" + bool_to_str(axis_setup.grid.major_enable));
                    params.push_back(
                        "major grid style={" + axis_setup.grid.major_thickness + ",color=" + axis_setup.grid.major_color
                        + "}");
                    params.push_back(
                        std::string(1, axis_kind) + "minorgrids=" + bool_to_str(axis_setup.grid.minor_enable));
                    params.push_back(
                        "minor grid style={" + axis_setup.grid.minor_thickness + ",color=" + axis_setup.grid.minor_color
                        + "}");
                    params.push_back(std::string(1, axis_kind) + "tick=" + (axis_setup.tick.enable ? "" : R"(\empty)"));
                    params.push_back(std::string(1, axis_kind_op) + R"(tick=\empty)");
                    params.push_back(
                        std::string(1, axis_kind) + "tick pos=" + (axis_setup.tick.opposite
                                                                       ? "both"
                                                                       : get_axis_pos(axis)));
                    params.push_back(
                        std::string(1, axis_kind) + "tick distance=" + (axis_setup.tick.major_distance
                                                                            ? fmt_flt(
                                                                                axis_setup.tick.major_distance.value())
                                                                            : ""));
                    params.push_back(
                        "major " + std::string(1, axis_kind) + " tick style={" + axis_setup.tick.major_thickness +
                        ",color=" + axis_setup.tick.major_color + "}");
                    params.push_back(
                        "minor " + std::string(1, axis_kind) + " tick style={" + axis_setup.tick.minor_thickness +
                        ",color=" + axis_setup.tick.minor_color + "}");
                    params.push_back(
                        "minor " + std::string(1, axis_kind) + " tick num=" + std::to_string(
                            axis_setup.tick.minor_num));

                    out.push_back(R"(\begin{axis}% )" + std::string(1, axis) + "-axis");
                    out.emplace_back("[");
                    for (const auto &p: params) out.push_back("  " + p + ",");
                    out.emplace_back("]");
                    out.emplace_back(R"(\end{axis})");
                }
                return out;
            }

            std::vector<std::string> create_plot_begin(char ax, char ay) {
                const auto &asy = fig->axes[ay].value();
                const auto &axis_setup = fig->axes[ax].value();
                auto params = get_axis_param('x', axis_setup);

                params.push_back("ymin=" + fmt_flt(asy.limits->first));
                params.push_back("ymax=" + fmt_flt(asy.limits->second));
                params.push_back("xmode=" + std::string(axis_setup.log ? "log" : "linear"));
                params.push_back("log basis x=" + axis_setup.log_base);
                params.push_back("ymode=" + std::string(asy.log ? "log" : "linear"));
                params.push_back("log basis y=" + asy.log_base);
                params.emplace_back("hide x axis=true");
                params.emplace_back("hide y axis=true");
                params.emplace_back(R"(xtick=\empty)");
                params.emplace_back(R"(ytick=\empty)");

                std::vector<std::string> out = {R"(\begin{axis})", "["};
                for (const auto &p: params) out.push_back("  " + p + ",");
                out.emplace_back("]");
                return out;
            }

            [[nodiscard]] std::vector<std::string> create_plot_content(char ax, char ay, const Data &data) const {
                const auto &asy = fig->axes[ay].value();
                const auto &asx = fig->axes[ax].value();
                auto y_domain = get_y_domain(asy);

                std::vector<std::string> params_plot = {
                    "color=" + data.ls.plot_color,
                    data.ls.line_style,
                    "line width=" + data.ls.line_width,
                    "mark=" + data.ls.marker,
                    "mark repeat=" + std::to_string(data.ls.marker_repeat),
                    "mark phase=" + std::to_string(data.ls.marker_phase),
                    "mark options={solid}"
                };

                if (data.ls.line_style.empty()) { params_plot.emplace_back("only marks"); }
                if (data.ls.marker.empty()) { params_plot.emplace_back("no markers"); }
                if (y_domain.has_value()) {
                    params_plot.push_back(
                        "restrict y to domain={" + fmt_flt(y_domain->first) + ":" + fmt_flt(y_domain->second) + "}");
                }

                std::vector<std::string> params_table = {
                    "row sep=newline", R"(x expr=\thisrowno{0}*)" + fmt_flt(asx.scale),
                    R"(y expr=\thisrowno{1}*)" + fmt_flt(asy.scale)
                };

                std::vector<std::string> out = {R"(\addplot [)"};
                for (const auto &p: params_plot) { out.push_back("  " + p + ","); }
                out.emplace_back("] table [");
                for (const auto &p: params_table) { out.push_back("  " + p + ","); }
                out.emplace_back("]{");

                for (std::size_t i = 0; i < data.dx.size(); ++i) {
                    out.push_back("  " + fmt_flt(data.dx[i]) + " " + fmt_flt(data.dy[i]));
                }
                out.emplace_back("};");
                out.push_back(R"(\label{dplot:)" + std::to_string(data._id) + "}");
                return out;
            }

            std::vector<std::string> create_plot_group(const char ax, const char ay) {
                std::vector<std::string> out = {
                    "", "%%%%%%%%%%%%%%%%%%", "% plot group " + std::string(1, ax) + "/" + std::string(1, ay) + " %",
                    "%%%%%%%%%%%%%%%%%%"
                };
                std::vector<Data> data_selected;

                for (const auto &data: fig->plot_data) {
                    char d_ax = (data.ax == XAxis::T) ? 't' : 'b';
                    char d_ay = (data.ay == YAxis::L) ? 'l' : 'r';
                    if (d_ax == ax && d_ay == ay) data_selected.push_back(data);
                }

                if (!data_selected.empty()) {
                    append(out, create_plot_begin(ax, ay));
                    for (const auto &data: data_selected) { append(out, create_plot_content(ax, ay, data)); }
                    out.emplace_back(R"(\end{axis})");
                }
                return out;
            }

            [[nodiscard]] std::vector<std::string> create_overlay() const {
                std::vector<std::string> out = {"", "%%%%%%%%%%%", "% overlay %", "%%%%%%%%%%%"};
                for (const auto &[axis, axis_setup_opt]: fig->axes) {
                    if (!axis_setup_opt.has_value()) continue;
                    const auto &axis_setup = axis_setup_opt.value();
                    const char axis_kind = get_axis_kind(axis);
                    const char axis_kind_op = get_opposite_axis_kind(axis_kind);
                    auto params = get_axis_param(axis_kind, axis_setup);

                    params.push_back(std::string(1, axis_kind_op) + "min=0");
                    params.push_back(std::string(1, axis_kind_op) + "max=1");
                    params.push_back(std::string(1, axis_kind) + "mode=" + (axis_setup.log ? "log" : "linear"));
                    params.push_back("log basis " + std::string(1, axis_kind) + "=" + axis_setup.log_base);
                    params.push_back(std::string(1, axis_kind) + "tick style={draw=none}");
                    params.push_back(
                        std::string(1, axis_kind) + "tick distance=" + (axis_setup.tick.major_distance
                                                                            ? fmt_flt(
                                                                                axis_setup.tick.major_distance.value())
                                                                            : ""));
                    params.push_back("hide " + std::string(1, axis_kind_op) + " axis=true");
                    params.push_back(std::string(1, axis_kind) + "ticklabel pos=" + get_axis_pos(axis));
                    params.emplace_back("axis on top=true");

                    out.push_back(R"(\begin{axis}% )" + std::string(1, axis) + "-axis");
                    out.emplace_back("[");
                    for (const auto &p: params) out.push_back("  " + p + ",");
                    out.emplace_back("]");
                    out.emplace_back(R"(\end{axis})");
                }
                return out;
            }

            [[nodiscard]] std::vector<std::string> create_legend() const {
                std::vector<std::string> out = {"", "%%%%%%%%%%", "% legend %", "%%%%%%%%%%"};
                std::vector<std::string> legend_style = {
                    "at={(" + fmt_flt(fig->legend_setup.at.first) + "," + fmt_flt(fig->legend_setup.at.second) + ")}",
                    "anchor=" + fig->legend_setup.anchor,
                    "legend cell align=" + fig->legend_setup.cell_align, "align=" + fig->legend_setup.align,
                    "nodes={scale=" + fmt_flt(fig->legend_setup.scale) + ", transform shape}"
                };

                auto params = get_axis_param('x', std::nullopt, std::make_pair(0.0, 1.0));
                params.emplace_back("ymin=0");
                params.emplace_back("ymax=1");
                params.emplace_back("xmode=linear");
                params.emplace_back("hide x axis=true");
                params.emplace_back("hide y axis=true");
                params.emplace_back("axis on top=true");

                std::string ls_join;
                for (size_t i = 0; i < legend_style.size(); ++i) {
                    ls_join += legend_style[i] + (i < legend_style.size() - 1 ? ", " : "");
                }
                params.push_back(R"(legend style={)" + ls_join + "}");

                out.emplace_back(R"(\begin{axis})");
                out.emplace_back("[");
                for (const auto &p: params) { out.push_back("  " + p + ","); }
                out.emplace_back("]");

                for (const auto &data: fig->plot_data) {
                    std::string label = data.label.empty() ? std::to_string(data._id) : data.label;
                    out.push_back(
                        R"(\addlegendimage{/pgfplots/refstyle=dplot:)" + std::to_string(data._id) +
                        R"(}\addlegendentry{)" + label + "}");
                }
                out.emplace_back(R"(\end{axis})");
                return out;
            }

        public:
            explicit LatexOutput(Figure *f) : fig(f) {
            }

            std::string exec() {
                std::vector<std::string> out;
                append(out, create_doc_begin());
                append(out, create_padding());
                append(out, create_background());

                std::vector<char> x_axes = {'t', 'b'};
                std::vector<char> y_axes = {'l', 'r'};

                for (char ax: x_axes) {
                    for (char ay: y_axes) { append(out, create_plot_group(ax, ay)); }
                }

                append(out, create_overlay());
                if (fig->legend_setup.enable) append(out, create_legend());

                out.emplace_back(R"(\end{tikzpicture})");
                out.emplace_back(R"(\end{document})");

                std::ostringstream joined;
                for (const auto &line: out) { joined << line << "\n"; }
                return joined.str();
            }
        };
    } // namespace

    // --- Data Implementation ---

    Data::Data(XAxis ax, YAxis ay, TypeData dx, TypeData dy, std::string label, LineSetup ls) : ax(ax), ay(ay),
        dx(std::move(dx)), dy(std::move(dy)), label(std::move(label)), ls(std::move(ls)) {
    }

    Data &Data::cfg_marker(double phase_frac, int n_samples, int n_markers) {
        if (n_samples == 0) n_samples = static_cast<int>(dx.size());
        ls.marker_repeat = static_cast<int>(std::floor(n_samples / n_markers));
        ls.marker_phase = static_cast<int>(std::round(std::fmod(phase_frac, 1.0) * n_samples / n_markers));
        return *this;
    }

    // --- Figure Implementation ---

    Figure::Figure(std::string name, std::string title) : name(std::move(name)), title(std::move(title)) {
        axes['t'] = std::nullopt;
        axes['b'] = std::nullopt;
        axes['l'] = std::nullopt;
        axes['r'] = std::nullopt;
    }

    void Figure::add(Data data) {
        data._id = _data_counter++;
        plot_data.push_back(std::move(data));
    }

    Data &Figure::plot(XAxis ax, YAxis ay, const TypeData &dx, const TypeData &dy, std::string label,
                       const std::optional<LineSetup> &ls) {
        Data d(ax, ay, dx, dy, std::move(label), ls.value_or(LineSetup()));
        add(std::move(d));
        return plot_data.back();
    }

    void Figure::_validate() {
        for (auto &[ax_key, axis_setup]: axes) {
            if (axis_setup && !axis_setup->limits.has_value()) {
                double mx = -std::numeric_limits<double>::max();
                double mn = std::numeric_limits<double>::max();

                for (const auto &data: plot_data) {
                    const char data_ax_char = (data.ax == XAxis::T) ? 't' : 'b';
                    const char data_ay_char = (data.ay == YAxis::L) ? 'l' : 'r';

                    if (data_ax_char == ax_key) {
                        mx = std::max(mx, axis_setup->scale * *std::max_element(data.dx.begin(), data.dx.end()));
                        mn = std::min(mn, axis_setup->scale * *std::min_element(data.dx.begin(), data.dx.end()));
                    }
                    if (data_ay_char == ax_key) {
                        mx = std::max(mx, axis_setup->scale * *std::max_element(data.dy.begin(), data.dy.end()));
                        mn = std::min(mn, axis_setup->scale * *std::min_element(data.dy.begin(), data.dy.end()));
                    }
                }
                axis_setup->limits = std::make_pair(mn, mx);
            }
        }
    }

    void Figure::show() {
        auto fig = matplot::figure(true);
        fig->name(title.empty() ? name : title);

        matplot::hold(matplot::on);

        for (const auto &data: plot_data) {
            const auto p = matplot::plot(data.dx, data.dy);

            if (!data.ls.plot_color.empty()) p->color(data.ls.plot_color);

            if (!data.ls.line_style.empty() && data.ls.line_style != "solid") {
                if (data.ls.line_style == "dashed")
                    p->line_style("--");
                else if (data.ls.line_style == "dotted")
                    p->line_style(":");
            }

            if (!data.ls.marker.empty()) p->marker(data.ls.marker);

            p->display_name(data.label);
        }

        if (axes['b'] && axes['b']->limits) { matplot::xlim({axes['b']->limits->first, axes['b']->limits->second}); }
        if (axes['l'] && axes['l']->limits) { matplot::ylim({axes['l']->limits->first, axes['l']->limits->second}); }

        if (axes['b'] && !axes['b']->label.empty()) matplot::xlabel(axes['b']->label);
        if (axes['l'] && !axes['l']->label.empty()) matplot::ylabel(axes['l']->label);

        if (legend_setup.enable) { matplot::legend(); }

        matplot::show();
    }

    void Figure::export_figure(const std::string &path_out_dir, const std::vector<ExportType> &types) {
        _validate();
        fs::path out_dir(path_out_dir);
        fs::create_directories(out_dir);

        fs::path path_latex = out_dir / (name + ".tex");
        fs::path path_pdf = out_dir / (name + ".pdf");

        bool need_latex = false, need_pdf = false;
        for (auto t: types) {
            if (t == ExportType::LATEX) need_latex = true;
            if (t == ExportType::PDF || t == ExportType::SVG) {
                need_pdf = true;
                need_latex = true;
            }
        }

        if (need_latex) {
            std::ofstream fp(path_latex);
            LatexOutput latex_gen(this);
            fp << latex_gen.exec();
            fp.close();
        }

        if (need_pdf) {
            std::string cmd = Environment::PATH_PDFLATEX + " -interaction=nonstopmode -output-directory=" + out_dir.
                              string() + " " + path_latex.string();
            int result = std::system(cmd.c_str());
            (void) result;
        }
    }
} // namespace dplot
