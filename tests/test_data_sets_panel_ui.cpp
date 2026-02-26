#include <gtest/gtest.h>

#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include "data_sets_panel.h"
#include "overlay_manager.h"

// Helper: build an OverlayManager with a base layer and N overlay files.
static std::shared_ptr<OverlayManager> makeManager(int overlay_count) {
  static PJ::PlotDataMapRef base_data;
  if (base_data.numeric.empty()) {
    base_data.addNumeric("sig");
  }

  auto mgr = std::make_shared<OverlayManager>();
  mgr->setBaseData(&base_data);
  std::string fixture = std::string(TEST_FIXTURE_DIR) + "/comma.csv";
  for (int i = 0; i < overlay_count; ++i) {
    mgr->loadOverlayFile(fixture);
  }
  return mgr;
}

// Trigger the named action in any open QMenu and close it.
// Returns true if the action was found.
static bool triggerMenuAction(const QString& text) {
  for (QWidget* w : QApplication::topLevelWidgets()) {
    if (auto* menu = qobject_cast<QMenu*>(w); menu && menu->isVisible()) {
      for (QAction* a : menu->actions()) {
        if (a->text() == text) {
          a->trigger();
          menu->close();
          return true;
        }
      }
      menu->close();
    }
  }
  return false;
}

// Refresh the panel (and process the resulting layout pass), then right-click
// on the first row and attempt to trigger the named action.
static bool rightClickAndTrigger(DataSetsPanel* panel,
                                 const std::shared_ptr<OverlayManager>& mgr,
                                 const QString& action) {
  panel->refresh(mgr);
  // Wait until the layout pass has run: the first child widget should be
  // visible and positioned at y > 0 (not the pre-layout default of y=0).
  QTest::qWaitFor(
      [panel]() -> bool {
        for (QObject* child : panel->children()) {
          if (auto* w = qobject_cast<QWidget*>(child); w && w->isVisible()) {
            return w->y() > 0;
          }
        }
        return false;
      },
      5000);

  bool triggered = false;
  QTimer::singleShot(50, [&]() { triggered = triggerMenuAction(action); });

  // Send a context-menu event directly: QTest::mouseClick(RightButton) does
  // not reliably generate QContextMenuEvent on all Linux platforms.
  // y=10 reliably lands inside the first row (top margin=2, row height≈15).
  QPoint pos(panel->width() / 2, 10);
  QContextMenuEvent ctx_event(QContextMenuEvent::Mouse, pos,
                              panel->mapToGlobal(pos));
  QApplication::sendEvent(panel, &ctx_event);
  QTest::qWait(100);
  return triggered;
}

class DataSetsPanelUITest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_panel = new DataSetsPanel;
    m_panel->resize(300, 80);
    m_panel->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(m_panel));
  }

  void TearDown() override { delete m_panel; }

  DataSetsPanel* m_panel = nullptr;
};

// --- clearOverlaysRequested ---

// "Clear All Overlays" is absent when no overlays are loaded (base only).
TEST_F(DataSetsPanelUITest, ClearAllOverlays_HiddenWithNoOverlays) {
  bool triggered =
      rightClickAndTrigger(m_panel, makeManager(0), "Clear All Overlays");
  EXPECT_FALSE(triggered);
}

// "Clear All Overlays" is present when at least one overlay is loaded.
TEST_F(DataSetsPanelUITest, ClearAllOverlays_VisibleWithOverlays) {
  bool triggered =
      rightClickAndTrigger(m_panel, makeManager(1), "Clear All Overlays");
  EXPECT_TRUE(triggered);
}

// Triggering "Clear All Overlays" emits clearOverlaysRequested exactly once.
TEST_F(DataSetsPanelUITest, ClearAllOverlays_EmitsSignal) {
  QSignalSpy spy(m_panel, &DataSetsPanel::clearOverlaysRequested);
  rightClickAndTrigger(m_panel, makeManager(2), "Clear All Overlays");
  EXPECT_EQ(spy.count(), 1);
}

// --- removeOverlayRequested ---

// "Remove Overlay" is absent when only the base layer is present.
TEST_F(DataSetsPanelUITest, RemoveOverlay_HiddenWithNoOverlays) {
  bool triggered =
      rightClickAndTrigger(m_panel, makeManager(0), "Remove Overlay");
  EXPECT_FALSE(triggered);
}

// "Remove Overlay" is present when at least one overlay is loaded.
TEST_F(DataSetsPanelUITest, RemoveOverlay_VisibleWithOverlays) {
  bool triggered =
      rightClickAndTrigger(m_panel, makeManager(1), "Remove Overlay");
  EXPECT_TRUE(triggered);
}

// Triggering "Remove Overlay" emits removeOverlayRequested exactly once.
TEST_F(DataSetsPanelUITest, RemoveOverlay_EmitsSignal) {
  QSignalSpy spy(m_panel, &DataSetsPanel::removeOverlayRequested);
  rightClickAndTrigger(m_panel, makeManager(1), "Remove Overlay");
  EXPECT_EQ(spy.count(), 1);
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
