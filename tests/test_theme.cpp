#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>

#include "plot_canvas.h"
#include "y_axis_panel.h"

// Replicates the mirrorLightness transform from signal_view_widget.cpp.
// L_new = 1.0 - L in HSL space; applying twice returns the original color.
static QColor mirrorLightness(const QColor& c) {
  qreal h, s, l, a;
  c.getHslF(&h, &s, &l, &a);
  return QColor::fromHslF(h, s, 1.0 - l, a);
}

// ============================================================================
// HSL lightness mirror math
// ============================================================================

// Applying mirrorLightness twice returns the original RGB values within ±1 per
// channel (from floating-point → integer rounding on each conversion).
TEST(MirrorLightness, IsInvertible) {
  const QColor samples[] = {
      QColor(0, 180, 255),    // Cyan-blue (L=0.5 — a no-op for L, but still
                              // round-trips cleanly via HSL)
      QColor(255, 100, 50),   // Orange-red
      QColor(50, 220, 100),   // Green
      QColor(255, 220, 50),   // Yellow
      QColor(200, 80, 255),   // Purple
      QColor(255, 100, 180),  // Pink
      QColor(100, 255, 220),  // Teal
      QColor(255, 255, 255),  // White
      QColor(0, 0, 0),        // Black
      QColor(128, 128, 128),  // Mid-gray
  };
  for (const auto& c : samples) {
    QColor rt = mirrorLightness(mirrorLightness(c));
    EXPECT_NEAR(rt.red(), c.red(), 1)
        << "R mismatch for " << c.name().toStdString();
    EXPECT_NEAR(rt.green(), c.green(), 1)
        << "G mismatch for " << c.name().toStdString();
    EXPECT_NEAR(rt.blue(), c.blue(), 1)
        << "B mismatch for " << c.name().toStdString();
  }
}

// Pure white (L=1) maps to pure black (L=0) and vice versa.
TEST(MirrorLightness, WhiteAndBlackSwap) {
  QColor white_mirrored = mirrorLightness(QColor(255, 255, 255));
  EXPECT_EQ(white_mirrored.red(), 0);
  EXPECT_EQ(white_mirrored.green(), 0);
  EXPECT_EQ(white_mirrored.blue(), 0);

  QColor black_mirrored = mirrorLightness(QColor(0, 0, 0));
  EXPECT_EQ(black_mirrored.red(), 255);
  EXPECT_EQ(black_mirrored.green(), 255);
  EXPECT_EQ(black_mirrored.blue(), 255);
}

// Colors with HSL lightness > 0.5 (typical dark-mode signal colors) become
// darker (lower Qt lightness) after mirroring.
TEST(MirrorLightness, BrightColorsBecomeDarker) {
  const QColor bright[] = {
      QColor(255, 100, 50),   // Orange-red  (L≈0.60)
      QColor(255, 220, 50),   // Yellow      (L≈0.60)
      QColor(200, 80, 255),   // Purple      (L≈0.66)
      QColor(255, 100, 180),  // Pink        (L≈0.70)
      QColor(100, 255, 220),  // Teal        (L≈0.70)
  };
  for (const auto& c : bright) {
    QColor m = mirrorLightness(c);
    EXPECT_LT(m.lightness(), c.lightness())
        << c.name().toStdString() << " should become darker";
  }
}

// ============================================================================
// PlotCanvas theme
// ============================================================================

TEST(PlotCanvasTheme, DefaultIsDark) {
  PlotCanvas canvas;
  EXPECT_EQ(canvas.theme(), Theme::Dark);
}

TEST(PlotCanvasTheme, SetLightReturnedByGetter) {
  PlotCanvas canvas;
  canvas.setTheme(Theme::Light);
  EXPECT_EQ(canvas.theme(), Theme::Light);
}

TEST(PlotCanvasTheme, ToggleRoundTrip) {
  PlotCanvas canvas;
  canvas.setTheme(Theme::Light);
  canvas.setTheme(Theme::Dark);
  EXPECT_EQ(canvas.theme(), Theme::Dark);
}

// setTheme(Light) writes a white window background to the Qt palette.
TEST(PlotCanvasTheme, LightThemeBackgroundIsWhite) {
  PlotCanvas canvas;
  canvas.setTheme(Theme::Light);
  EXPECT_EQ(canvas.palette().color(QPalette::Window), QColor(255, 255, 255));
}

// setTheme(Dark) writes a dark window background to the Qt palette.
TEST(PlotCanvasTheme, DarkThemeBackgroundIsDark) {
  PlotCanvas canvas;
  canvas.setTheme(Theme::Light);  // switch away first
  canvas.setTheme(Theme::Dark);
  EXPECT_EQ(canvas.palette().color(QPalette::Window), QColor(30, 30, 30));
}

// ============================================================================
// YAxisBarColumn theme (smoke: no crash, update() called without error)
// ============================================================================

TEST(YAxisBarTheme, SetThemeDoesNotCrash) {
  YAxisBarColumn bar;
  EXPECT_NO_FATAL_FAILURE(bar.setTheme(Theme::Light));
  EXPECT_NO_FATAL_FAILURE(bar.setTheme(Theme::Dark));
}

// SignalColumnBase (via SignalNameColumn) — same smoke test.
TEST(SignalNameTheme, SetThemeDoesNotCrash) {
  SignalNameColumn col;
  EXPECT_NO_FATAL_FAILURE(col.setTheme(Theme::Light));
  EXPECT_NO_FATAL_FAILURE(col.setTheme(Theme::Dark));
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
