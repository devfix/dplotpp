//
// Created by Tristan Krause on 2026-04-29.
//

#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dplot {
    enum class XAxis {
        T,
        B
    };

    enum class YAxis {
        L,
        R
    };

    enum class ExportType {
        LATEX,
        PDF,
        SVG
    };

    //using TypeData = nc::NdArray<double>;
    using TypeData = std::vector<double>;

    // Environment Constants Declaration
    struct Environment {
        static const std::string PATH_PDFLATEX;
        static const std::string PATH_PDF2SVG;
        static const std::string PATH_SCOUR;
    };

    // Configuration Structs
    struct GridSetup {
        bool major_enable = false;
        std::string major_thickness = "thin";
        std::string major_color = "black";
        bool minor_enable = false;
        std::string minor_color = "black";
        std::string minor_thickness = "very thin";
    };

    struct TickSetup {
        bool enable = true;
        bool opposite = false;
        std::string major_thickness = "thin";
        std::string major_color = "black";
        std::optional<double> major_distance = std::nullopt;
        std::string minor_thickness = "thin";
        std::string minor_color = "gray";
        int minor_num = 0;
    };

    struct LegendSetup {
        bool enable = true;
        std::string anchor = "north east";
        std::string align = "left";
        std::string cell_align = "left";
        std::pair<double, double> at = {0.98, 0.98};
        double scale = 0.8;
    };

    struct AxisSetup {
        std::string label;
        std::string label_shift = "0cm";
        double scale = 1.0;
        bool log = false;
        std::string log_base = "10";
        std::optional<std::pair<double, double> > limits = std::nullopt;
        std::string padding = "0cm";
        GridSetup grid;
        TickSetup tick;
    };

    struct LineSetup {
        std::string plot_color = "black";
        std::string line_style = "solid";
        std::string line_width = "1pt";
        std::string marker;
        int marker_repeat = 1;
        int marker_phase = 0;
    };

    // Data Class Declaration
    class Data {
    public:
        XAxis ax;
        YAxis ay;
        TypeData dx;
        TypeData dy;
        std::string label;
        LineSetup ls;
        int _id = -1;

        Data(XAxis ax, YAxis ay, TypeData dx, TypeData dy, std::string label = "", LineSetup ls = LineSetup());

        Data &cfg_marker(double phase_frac = 0.0, int n_samples = 0, int n_markers = 5);
    };

    // Figure Class Declaration
    class Figure {
    public:
        std::string name;
        std::string title;
        std::string width = "5cm";
        std::string height = "5cm";
        std::string basic_thickness = "thick";
        std::string background_color = "white";
        LegendSetup legend_setup;

        std::map<char, std::optional<AxisSetup> > axes;
        std::vector<Data> plot_data;

    private:
        int _data_counter = 0;

        void _validate();

    public:
        explicit Figure(std::string name, std::string title = "");

        void add(Data data);

        Data &plot(XAxis ax, YAxis ay, const TypeData &dx, const TypeData &dy, std::string label = "",
                   const std::optional<LineSetup> &ls = std::nullopt);

        void show();

        void export_figure(const std::string &path_out_dir, const std::vector<ExportType> &types);
    };
} // namespace dplot
