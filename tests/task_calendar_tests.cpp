#include "Calendar/CalendarWindow.h"
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
class TaskCalendarTests:public QObject {
 Q_OBJECT
 QString directory()const{return QCoreApplication::arguments().at(2);}
 QString path()const{return directory()+"/tasks.json";}
 TaskEntry sample(int count){TaskEntry task;task.id="task";task.name=QStringLiteral("测试任务");for(int i=0;i<count;i++){TaskNode node;node.id=QString::number(i);node.name=QStringLiteral("节点%1").arg(i);node.date=i%5?QDate::currentDate().addDays(i):QDate();node.completed=i%3==0;task.nodes.append(node);}return task;}
private slots:
 void init(){QFile::remove(path());QFile::remove(directory()+"/settings.ini");}
 void modelRoundTrip(){auto task=sample(37);QString error;QVector<TaskEntry> loaded;auto data=TaskData::encode({task});QVERIFY(!data.contains("color"));QVERIFY(TaskData::decode(data,loaded,error));QCOMPARE(loaded.size(),1);QCOMPARE(loaded[0].nodes.size(),37);QCOMPARE(loaded[0].nodes[10].name,QStringLiteral("节点10"));QVERIFY(!loaded[0].nodes[0].date.isValid());QCOMPARE(loaded[0].completedNodes(),13);}
 void datesAndCountdown(){QCOMPARE(TaskData::parseDate("2026/1/2"),QDate(2026,1,2));QCOMPARE(TaskData::parseDate("20261115"),QDate(2026,11,15));QVERIFY(!TaskData::parseDate("2026-02-29").isValid());QCOMPARE(TaskData::countdown(QDate(2028,3,1),QDate(2028,2,28)),QStringLiteral("还有 2 天"));QCOMPARE(TaskData::countdown(QDate(2026,1,1),QDate(2026,1,1)),QStringLiteral("今天"));QCOMPARE(TaskData::countdown(QDate(),QDate::currentDate()),QStringLiteral("日期待定"));QCOMPARE(TaskData::countdown(QDate(),QDate::currentDate(),true),QStringLiteral("已完成"));}
 void rejectsCorruptModel(){auto task=sample(2);task.nodes[1].id=task.nodes[0].id;QString error;QVector<TaskEntry> loaded;QVERIFY(!TaskData::decode(TaskData::encode({task}),loaded,error));QVERIFY(loaded.isEmpty());QVERIFY(!TaskData::decode("{broken",loaded,error));}
 void manyNodesLayout(){TaskEntry task=sample(400);for(int i=0;i<task.nodes.size();i++)task.nodes[i].date=QDate::currentDate().addDays(i%5);for(double width:{220.,440.,1000.}){auto labels=Timeline::layoutLabels(task,QDate::currentDate().addDays(-22),90,width);int count=0;for(int i=0;i<labels.size();i++){count+=labels[i].nodes.size();QVERIFY(labels[i].rect.left()>=0);QVERIFY(labels[i].rect.right()<=width);for(int j=i+1;j<labels.size();j++)QVERIFY(!labels[i].rect.intersects(labels[j].rect));}QCOMPARE(count,400);QVERIFY(labels.size()<20);}}
 void sparseLayoutKeepsAllNodes(){auto task=sample(12);for(int i=0;i<12;i++)task.nodes[i].date=QDate::currentDate().addDays(11-i);auto labels=Timeline::layoutLabels(task,QDate::currentDate().addDays(-1),30,1000);int count=0;for(auto label:labels)count+=label.nodes.size();QCOMPARE(count,12);}
 void editArbitraryNodes(){CalendarWindow window;window.show();QTimer::singleShot(0,[]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->findChild<QLineEdit*>("taskName")->setText(QStringLiteral("十二节点任务"));auto table=dialog->findChild<QTableWidget*>("nodesTable");QVERIFY(table);QCOMPARE(table->columnCount(),4);auto add=dialog->findChild<QPushButton*>("addNodeButton");for(int i=0;i<12;i++)add->click();table->setCurrentCell(0,0);QCoreApplication::processEvents();QCOMPARE(table->rowCount(),12);for(int i=0;i<12;i++){table->item(i,1)->setText(QString("Node%1").arg(i));if(i!=3)table->cellWidget(i,2)->findChild<QLineEdit*>("nodeDate")->setText(QDate::currentDate().addDays(12-i).toString(Qt::ISODate));}dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});window.findChild<QPushButton*>("addButton")->click();CalendarWindow restored;auto t=restored.findChild<Timeline*>();QCOMPARE(t->tasks.size(),1);QCOMPARE(t->tasks[0].nodes.size(),12);QVERIFY(!t->tasks[0].nodes[3].date.isValid());}
 void completionAndOrderPersist(){CalendarWindow w;auto t=w.findChild<Timeline*>();auto a=sample(2);for(auto &n:a.nodes)n.completed=false;auto b=sample(0);b.id="second";t->tasks={a,b};QVERIFY(w.save());t->nodeToggled(0,0,true);QVERIFY(!t->tasks[0].isComplete());t->nodeToggled(0,1,true);QVERIFY(t->tasks[0].isComplete());t->reorderRequested(0,1);CalendarWindow restored;auto loaded=restored.findChild<Timeline*>();QCOMPARE(loaded->tasks[1].id,QString("task"));QVERIFY(loaded->tasks[1].isComplete());loaded->taskToggled(1,false);QCOMPARE(loaded->tasks[1].completedNodes(),0);}
 void corruptedFileProtected(){QFile file(path());QVERIFY(file.open(QIODevice::WriteOnly));file.write("bad json");file.close();CalendarWindow w;QVERIFY(!w.load());QTimer::singleShot(0,[]{if(auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->accept();});QVERIFY(!w.save());QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("bad json"));}
 void stickyHeaderAndWheelRegions(){
 CalendarWindow w;auto t=w.findChild<Timeline*>();auto scroll=w.findChild<QScrollArea*>("taskScroll");
 for(int i=0;i<20;i++){auto task=sample(3);task.id=QString::number(i);t->tasks.append(task);}t->refresh();w.resize(580,250);w.show();QTest::qWait(30);
 auto wheel=[&](int x,Qt::KeyboardModifiers mods=Qt::NoModifier){QPointF point(x,t->scrollOffset()+70);QWheelEvent e(point,t->mapToGlobal(point.toPoint()),QPoint(),QPoint(0,-120),Qt::NoButton,mods,Qt::NoScrollPhase,false);QApplication::sendEvent(t,&e);QCoreApplication::processEvents();QVERIFY(e.isAccepted());};
 QDate old=t->start;int span=t->days;wheel(60);QVERIFY(scroll->verticalScrollBar()->value()>0);QCOMPARE(t->start,old);QCOMPARE(t->days,span);QCOMPARE(t->scrollOffset(),scroll->verticalScrollBar()->value());
 int vertical=scroll->verticalScrollBar()->value();wheel(400);QVERIFY(t->start>old);QCOMPARE(scroll->verticalScrollBar()->value(),vertical);
 wheel(400,Qt::ShiftModifier);QCOMPARE(scroll->verticalScrollBar()->value(),vertical);
 wheel(400,Qt::ControlModifier);QVERIFY(t->days>span);QCOMPARE(scroll->verticalScrollBar()->value(),vertical);
 old=t->start;span=t->days;wheel(60,Qt::ControlModifier);QCOMPARE(t->start,old);QCOMPARE(t->days,span);
 t->start=QDate::currentDate().addDays(7);t->refresh();t->grab();auto button=t->findChild<QPushButton*>("todayOverflow");QVERIFY(button->isVisible());QCOMPARE(button->y(),t->scrollOffset());button->click();QCOMPARE(t->start,QDate::currentDate().addDays(-t->days/4));
 QSignalSpy editor(t,&Timeline::editRequested);QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(70,t->scrollOffset()+20));QCOMPARE(editor.count(),0);
 QVERIFY(w.grab().save(QDir::currentPath()+"/sticky-header-preview.png"));
 }
 void holdLabelToggle(){
 CalendarWindow w;auto t=w.findChild<Timeline*>();auto task=sample(1);task.nodes[0].date=QDate::currentDate().addDays(10);task.nodes[0].completed=false;t->tasks={task};t->refresh();QVERIFY(w.save());w.show();QTest::qWait(30);
 auto labels=Timeline::layoutLabels(task,t->start,t->days,t->width()-Timeline::NameWidth);QCOMPARE(labels.size(),1);QPoint point=labels[0].rect.translated(Timeline::NameWidth,t->rowTop(0)).center().toPoint();QSignalSpy toggles(t,&Timeline::nodeToggled);QTest::mouseMove(t,point);QTest::qWait(30);
 QTest::mousePress(t,Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(250);QCOMPARE(toggles.count(),0);QVERIFY(t->grab().save(QDir::currentPath()+"/hold-progress-preview.png"));QTest::qWait(300);QCOMPARE(toggles.count(),1);QVERIFY(t->tasks[0].nodes[0].completed);QTest::qWait(550);QCOMPARE(toggles.count(),1);QTest::mouseRelease(t,Qt::LeftButton,Qt::NoModifier,point);QVERIFY(!t->findChild<QMenu*>("nodeDetails"));
 QTest::mousePress(t,Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(550);QTest::mouseRelease(t,Qt::LeftButton,Qt::NoModifier,point);QCOMPARE(toggles.count(),2);QVERIFY(!t->tasks[0].nodes[0].completed);
 QTest::mousePress(t,Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(100);QTest::mouseMove(t,QPoint(15,point.y()));QTest::qWait(550);QTest::mouseRelease(t,Qt::LeftButton,Qt::NoModifier,QPoint(15,point.y()));QCOMPARE(toggles.count(),2);
 QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,point);auto menu=t->findChild<QMenu*>("nodeDetails");QVERIFY(menu);QTest::keyClick(menu,Qt::Key_Escape);
 CalendarWindow restored;QVERIFY(!restored.findChild<Timeline*>()->tasks[0].nodes[0].completed);
 }
 void staggeredLabelsOnlyShowNames(){
 TaskEntry task;for(int i=0;i<3;i++){TaskNode n;n.id=QString::number(i);n.name=QStringLiteral("完整名称%1").arg(i);n.date=QDate(2026,1,1).addDays(30+i*6);task.nodes.append(n);}
 auto labels=Timeline::layoutLabels(task,QDate(2026,1,1),100,800);QCOMPARE(labels.size(),3);bool lower=false;for(const auto &l:labels){QVERIFY(l.subtitle.isEmpty());QCOMPARE(l.rect.height(),18.0);QCOMPARE(l.title,task.nodes[l.nodes[0]].name);lower|=l.rect.top()>9;}QVERIFY(lower);for(int i=0;i<labels.size();i++)for(int j=i+1;j<labels.size();j++)QVERIFY(!labels[i].rect.intersects(labels[j].rect));
 auto sparse=Timeline::layoutLabels(task,QDate(2026,1,1),50,1400);QCOMPARE(sparse.size(),3);for(const auto &l:sparse){QVERIFY(!l.subtitle.isEmpty());QCOMPARE(l.rect.height(),27.0);QCOMPARE(l.rect.top(),9.0);}
 }
 void wheelAndToday(){Timeline t;t.resize(580,150);t.show();QDate old=t.start;QWheelEvent pan(QPointF(400,70),QPointF(400,70),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&t,&pan);QVERIFY(t.start>old);QCOMPARE(t.days,90);QDate center=t.start.addDays(t.days/2);QWheelEvent zoom(QPointF(400,70),QPointF(400,70),QPoint(),QPoint(0,120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&t,&zoom);QVERIFY(t.days<90);QCOMPARE(t.start.addDays(t.days/2),center);t.start=QDate::currentDate().addDays(5);t.refresh();t.grab();auto button=t.findChild<QPushButton*>("todayOverflow");QVERIFY(button->isVisible());button->click();QCOMPARE(t.start,QDate::currentDate().addDays(-t.days/4));}
 void clusteredPopup(){CalendarWindow w;auto t=w.findChild<Timeline*>();auto task=sample(8);for(auto &n:task.nodes){n.date=QDate::currentDate().addDays(5);n.completed=false;}t->tasks={task};t->refresh();QVERIFY(w.save());w.show();QCoreApplication::processEvents();int x=Timeline::NameWidth+qRound(t->start.daysTo(task.nodes[0].date)*double(t->width()-Timeline::NameWidth)/t->days);QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(x,t->rowTop(0)+4));QPointer<QMenu> menu=t->findChild<QMenu*>("nodeDetails");QVERIFY(menu);QVERIFY(menu->isVisible());auto checks=menu->findChildren<QCheckBox*>();QCOMPARE(checks.size(),8);checks[0]->click();QVERIFY(t->tasks[0].nodes[0].completed);QTest::keyClick(menu,Qt::Key_Escape);QVERIFY(!menu||!menu->isVisible());}
 void lockPersists(){CalendarWindow w;auto lock=w.findChild<QPushButton*>("lockButton");QVERIFY(lock->text().isEmpty());lock->click();CalendarWindow restored;QVERIFY(restored.findChild<QPushButton*>("lockButton")->isChecked());QVERIFY(w.testAttribute(Qt::WA_TranslucentBackground));}
 void snapshot(){CalendarWindow w;auto t=w.findChild<Timeline*>();t->tasks={sample(9),sample(3)};t->tasks[0].name=QStringLiteral("自定义项目");t->tasks[1].id="second";t->tasks[1].name=QStringLiteral("密集节点");for(int i=0;i<9;i++){t->tasks[0].nodes[i].date=QDate::currentDate().addDays(-7+i*8);t->tasks[0].nodes[i].name=QStringLiteral("第%1步").arg(i+1);}for(auto &n:t->tasks[1].nodes)n.date=QDate::currentDate().addDays(3);t->refresh();QVERIFY(w.save());w.show();QTest::qWait(50);QVERIFY(w.grab().save(QDir::currentPath()+"/task-preview.png"));}
};
int main(int argc,char **argv){QTemporaryDir directory(QDir::currentPath()+"/task-tests-XXXXXX");if(!directory.isValid())return 2;QByteArray exe=argv[0],flag="--data-dir",path=directory.path().toUtf8();char *args[]={exe.data(),flag.data(),path.data(),nullptr};int count=3;QApplication app(count,args);QCoreApplication::setOrganizationName("TaskCalendarTests");QCoreApplication::setApplicationName("TaskCalendarTests");
#ifdef Q_OS_WIN
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
#endif
 TaskCalendarTests tests;return QTest::qExec(&tests,QStringList{"TaskCalendarTests","-o",QDir::currentPath()+"/task-test-results.txt,txt"});}
#include "task_calendar_tests.moc"


