#pragma once
#include <QtWidgets>
#include "TaskModel.h"
struct NodeLabel {QVector<int> nodes;double anchor=0;QRectF rect;QString title,subtitle;};
struct TaskRow {int top=0,height=42;QVector<NodeLabel> labels;};
class Timeline : public QWidget {
 Q_OBJECT
public:
 explicit Timeline(QWidget *parent=nullptr);
 QVector<TaskEntry> tasks;
 QDate start=QDate::currentDate().addDays(-22);
 int days=90,selected=-1;
 static constexpr int NameWidth=140,AxisHeight=32;
 static QVector<NodeLabel> layoutLabels(const TaskEntry&,QDate,int,double width);
 static QColor tint(QColor);
 static QColor ink(QColor);
 void refresh();
 void setScrollOffset(int value);
 int scrollOffset() const {return headerOffset;}
 int rowTop(int row) const;
 int rowHeight(int row) const;
signals:
 void editRequested(int index);
 void reorderRequested(int from,int to);
 void taskToggled(int index,bool value);
 void nodeToggled(int task,int node,bool value);
 void scrollRequested(int delta);
protected:
 void paintEvent(QPaintEvent*) override;
 void resizeEvent(QResizeEvent*) override;
 void mousePressEvent(QMouseEvent*) override;
 void mouseReleaseEvent(QMouseEvent*) override;
 void mouseMoveEvent(QMouseEvent*) override;
 void wheelEvent(QWheelEvent*) override;
 void keyPressEvent(QKeyEvent*) override;
 void dragEnterEvent(QDragEnterEvent*) override;
 void dragMoveEvent(QDragMoveEvent*) override;
 void dragLeaveEvent(QDragLeaveEvent*) override;
 void dropEvent(QDropEvent*) override;
 bool event(QEvent*) override;
private:
 QTimer holdTimer;
 QElapsedTimer holdElapsed;
 QRectF holdRect;
 int holdRow=-1,holdNode=-1;
 QString holdTaskId,holdNodeId;
 bool holdConsumed=false;
 void cancelHold();
 int headerOffset=0;
 QVector<TaskRow> rows;
 QPushButton *todayOverflow;
 QPoint press;
 int pressedRow=-1,insertion=-1;
 int rowAt(int y) const;
 int insertionAt(int y) const;
 QVector<int> nodesAt(QPoint) const;
 void showNodes(int task,const QVector<int>&,QPoint global);
 bool validDrag(const QMimeData*) const;
};
class CalendarWindow : public QWidget {
 Q_OBJECT
public:
 CalendarWindow();
 bool load();
 bool save();
protected:
 void closeEvent(QCloseEvent*) override;
 void paintEvent(QPaintEvent*) override;
 bool eventFilter(QObject*,QEvent*) override;
#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
 bool nativeEvent(const QByteArray&,void*,qintptr*) override;
#else
 bool nativeEvent(const QByteArray&,void*,long*) override;
#endif
#endif
private:
 Timeline *timeline;
 QLabel *status;
 QPushButton *lockButton;
 QSettings settings;
 QString dataPath;
 bool writable=true,locked=false;
 void edit(int index);
 void reorder(int from,int to);
 void setLocked(bool);
 void updateStatus();
};
