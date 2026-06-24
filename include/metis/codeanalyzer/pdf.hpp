/* Metis Code Analyzer Plus - minimal PDF writer
 * Pure C++20, zero external dependencies, 7-bit ASCII output.
 * Emits PDF 1.4 with the standard Helvetica fonts (no embedding), text, and
 * filled rectangles, with automatic pagination. Intended for report export.
 */
#ifndef METIS_CODE_ANALYZER_PDF_HPP
#define METIS_CODE_ANALYZER_PDF_HPP

#include <sstream>
#include <string>
#include <vector>

namespace metis::codeanalyzer::pdf {

class PdfBuilder {
    /* Builds a PDF 1.4 document incrementally using Helvetica type-1 fonts.
     * Call heading() and kv() to add content, then build() to get the bytes.
     * New pages are inserted automatically when content overflows the page
     * height margin. All output is 7-bit ASCII (PDF numeric/hex encoding). */
public:
    explicit PdfBuilder(double page_w = 612.0, double page_h = 792.0,
                        double margin = 54.0)
        : page_w_(page_w), page_h_(page_h), margin_(margin) {
        new_page();
    }

    void new_page() {
        if (!current_.str().empty()) pages_.push_back(current_.str());
        current_.str("");
        current_.clear();
        y_ = page_h_ - margin_;
    }

    /* Append a bold section heading, advancing the cursor and auto-paginating. */
    void heading(const std::string& s) {
        ensure(28.0);
        text(true, 16.0, margin_, y_, s);
        y_ -= 22.0;
    }

    void subheading(const std::string& s) {
        ensure(22.0);
        text(true, 12.0, margin_, y_, s);
        y_ -= 17.0;
    }

    void line(const std::string& s) {
        ensure(15.0);
        text(false, 10.0, margin_, y_, s);
        y_ -= 14.0;
    }

    /* Append a key: value label pair in regular weight text. */
    void kv(const std::string& key, const std::string& value) {
        ensure(15.0);
        text(false, 10.0, margin_, y_, key);
        text(false, 10.0, margin_ + 200.0, y_, value);
        y_ -= 14.0;
    }

    void gap(double pts) { ensure(pts); y_ -= pts; }

    /* A big grade letter with its score, color given as RGB 0..1. */
    void grade_block(const std::string& letter, const std::string& score,
                     double r, double g, double b) {
        ensure(70.0);
        rect(margin_, y_ - 54.0, 64.0, 64.0, r, g, b);
        text_color(true, 40.0, margin_ + 18.0, y_ - 44.0, letter, 1, 1, 1);
        text(true, 14.0, margin_ + 84.0, y_ - 16.0, "Total Quality Index");
        text(false, 12.0, margin_ + 84.0, y_ - 36.0, score);
        y_ -= 74.0;
    }

    /* A labeled horizontal bar (percent 0..100) with colored fill. */
    void bar(const std::string& label, double pct,
             double r, double g, double b) {
        ensure(20.0);
        text(false, 10.0, margin_, y_, label);
        double bx = margin_ + 160.0;
        double bw = page_w_ - margin_ - bx - 40.0;
        rect(bx, y_ - 2.0, bw, 9.0, 0.86, 0.89, 0.94);
        rect(bx, y_ - 2.0, bw * (pct / 100.0), 9.0, r, g, b);
        std::ostringstream v;
        v.precision(0);
        v << std::fixed << pct;
        text(false, 10.0, bx + bw + 8.0, y_, v.str());
        y_ -= 16.0;
    }

    /* Finalize the document and return the complete PDF byte string. */
    std::string build() {
        if (!current_.str().empty()) { pages_.push_back(current_.str()); current_.str(""); }
        if (pages_.empty()) pages_.push_back("");

        std::vector<std::string> objects;
        /* 1 Catalog, 2 Pages, 3 Font Helvetica, 4 Font Helvetica-Bold */
        objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");

        std::size_t page_count = pages_.size();
        std::ostringstream kids;
        for (std::size_t i = 0; i < page_count; ++i) {
            if (i) kids << ' ';
            kids << (5 + i * 2) << " 0 R";
        }
        std::ostringstream pages_obj;
        pages_obj << "<< /Type /Pages /Kids [" << kids.str()
                  << "] /Count " << page_count << " >>";
        objects.push_back(pages_obj.str());

        objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
        objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>");

        for (std::size_t i = 0; i < page_count; ++i) {
            std::ostringstream page_obj;
            page_obj << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
                     << page_w_ << ' ' << page_h_ << "] "
                     << "/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
                     << "/Contents " << (6 + i * 2) << " 0 R >>";
            objects.push_back(page_obj.str());

            const std::string& stream = pages_[i];
            std::ostringstream content_obj;
            content_obj << "<< /Length " << stream.size() << " >>\nstream\n"
                        << stream << "endstream";
            objects.push_back(content_obj.str());
        }

        std::ostringstream out;
        out << "%PDF-1.4\n";
        std::vector<std::size_t> offsets(objects.size() + 1, 0);
        for (std::size_t i = 0; i < objects.size(); ++i) {
            offsets[i + 1] = static_cast<std::size_t>(out.tellp());
            out << (i + 1) << " 0 obj\n" << objects[i] << "\nendobj\n";
        }
        std::size_t xref_pos = static_cast<std::size_t>(out.tellp());
        out << "xref\n0 " << (objects.size() + 1) << "\n";
        out << "0000000000 65535 f \n";
        for (std::size_t i = 1; i <= objects.size(); ++i) {
            out << pad10(offsets[i]) << " 00000 n \n";
        }
        out << "trailer\n<< /Size " << (objects.size() + 1)
            << " /Root 1 0 R >>\nstartxref\n" << xref_pos << "\n%%EOF\n";
        return out.str();
    }

private:
    void ensure(double needed) {
        if (y_ - needed < margin_) new_page();
    }

    static std::string pad10(std::size_t v) {
        std::string s = std::to_string(v);
        if (s.size() < 10) s = std::string(10 - s.size(), '0') + s;
        return s;
    }

    static std::string escape(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '(' || c == ')' || c == '\\') out.push_back('\\');
            if (static_cast<unsigned char>(c) >= 0x20 &&
                static_cast<unsigned char>(c) < 0x7F) {
                out.push_back(c);
            } else {
                out.push_back(' ');  /* keep output 7-bit and PDF-safe */
            }
        }
        return out;
    }

    void text(bool bold, double size, double x, double y, const std::string& s) {
        text_color(bold, size, x, y, s, 0.09, 0.13, 0.20);
    }

    void text_color(bool bold, double size, double x, double y,
                    const std::string& s, double r, double g, double b) {
        current_ << r << ' ' << g << ' ' << b << " rg\n";
        current_ << "BT /" << (bold ? "F2" : "F1") << ' ' << size << " Tf "
                 << x << ' ' << y << " Td (" << escape(s) << ") Tj ET\n";
    }

    void rect(double x, double y, double w, double h,
              double r, double g, double b) {
        current_ << r << ' ' << g << ' ' << b << " rg\n"
                 << x << ' ' << y << ' ' << w << ' ' << h << " re f\n";
    }

    double page_w_;
    double page_h_;
    double margin_;
    double y_ = 0.0;
    std::ostringstream current_;
    std::vector<std::string> pages_;
};

} /* namespace metis::codeanalyzer::pdf */

#endif /* METIS_CODE_ANALYZER_PDF_HPP */
