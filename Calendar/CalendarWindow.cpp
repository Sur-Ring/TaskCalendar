#include "CalendarWindow.h"
#include "DesktopController.h"
#include <QSaveFile>
#include <QUuid>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif
namespace {
QString dataDirectory(){auto args=QCoreApplication::arguments();int index=args.indexOf("--data-dir");return index>=0&&index+1<args.size()?QDir(args[index+1]).absolutePath():QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);}
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
QIcon lockIcon(bool locked){QIcon icon;for(int size:{24,48,72}){QPixmap pixmap(size,size);pixmap.fill(Qt::transparent);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);p.scale(size/24.0,size/24.0);p.setPen(QPen(QColor("#eef2ef"),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);p.drawRoundedRect(QRectF(5,10,14,11),2,2);QPainterPath path;double left=locked?8:10,right=locked?16:18;path.moveTo(left,10);path.lineTo(left,6);path.cubicTo(left,1,right,1,right,6);if(locked)path.lineTo(right,10);p.drawPath(path);p.drawEllipse(QPointF(12,15),.8,.8);p.drawLine(QPointF(12,16),QPointF(12,18));p.end();icon.addPixmap(pixmap);}return icon;}
}
CalendarWindow::CalendarWindow():settings(dataDirectory()+"/settings.ini",QSettings::IniFormat){
 setWindowTitle(QStringLiteral("TaskCalendar · 任务时间轴"));setWindowFlags(Qt::Tool|Qt::FramelessWindowHint);setAttribute(Qt::WA_QuitOnClose,true);setAttribute(Qt::WA_TranslucentBackground);resize(580,300);setMinimumSize(460,190);
 setStyleSheet("QWidget{background:transparent;color:#eef2ef;font-family:'Microsoft YaHei UI';font-size:12px} QPushButton,QToolButton{padding:4px 7px;border:1px solid rgba(255,255,255,68);border-radius:4px;background:rgba(255,255,255,28)} QPushButton:hover{background:rgba(255,255,255,55)} QPushButton:checked{background:rgba(140,180,140,70);border-color:#a9c3a1} QScrollArea{border:1px solid rgba(255,255,255,45)}");
 auto root=new QVBoxLayout(this);root->setContentsMargins(10,8,10,7);root->setSpacing(5);auto header=new QWidget;header->setObjectName("dragHeader");header->installEventFilter(this);auto head=new QHBoxLayout(header);head->setContentsMargins(0,0,0,0);head->setSpacing(5);auto title=new QLabel(QStringLiteral("任务时间轴"));title->setObjectName("dragHeader");title->setStyleSheet("font-size:14px;font-weight:600");title->installEventFilter(this);head->addWidget(title,1);
 lockButton=new QPushButton;lockButton->setObjectName("lockButton");lockButton->setFixedSize(30,26);lockButton->setIconSize(QSize(18,18));lockButton->setCheckable(true);head->addWidget(lockButton);auto add=new QPushButton(QStringLiteral("＋ 任务"));add->setObjectName("addButton");head->addWidget(add);auto close=new QPushButton("×");close->setToolTip(QStringLiteral("退出"));close->setFixedWidth(24);head->addWidget(close);root->addWidget(header);
 timeline=new Timeline;auto scroll=new QScrollArea;scroll->setObjectName("taskScroll");scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setWidgetResizable(true);scroll->setWidget(timeline);scroll->viewport()->setAutoFillBackground(false);timeline->setAutoFillBackground(false);root->addWidget(scroll,1);auto footer=new QHBoxLayout;status=new QLabel;status->setObjectName("taskStatus");status->setWordWrap(true);status->setStyleSheet("font-size:10px;color:#c5cec7");footer->addWidget(status,1);auto help=new QLabel(QStringLiteral("长按标签 0.5 秒切换完成 · 滚轮平移 · Ctrl + 滚轮缩放"));help->setStyleSheet("font-size:10px;color:#c5cec7");help->setToolTip(QStringLiteral("名称编辑；节点点击展开；拖柄排序；左侧滚轮上下滚动"));footer->addWidget(help);root->addLayout(footer);
 connect(scroll->verticalScrollBar(),&QScrollBar::valueChanged,timeline,&Timeline::setScrollOffset);
 dataPath=dataDirectory()+"/tasks.json";load();timeline->days=qBound(14,settings.value("days",90).toInt(),730);timeline->start=QDate::currentDate().addDays(-timeline->days/4);timeline->refresh();restoreGeometry(settings.value("geometry").toByteArray());setLocked(settings.value("locked",false).toBool());new DesktopController(this);
 connect(lockButton,&QPushButton::toggled,this,&CalendarWindow::setLocked);connect(close,&QPushButton::clicked,this,&QWidget::close);connect(add,&QPushButton::clicked,this,[this]{edit(-1);});connect(timeline,&Timeline::editRequested,this,&CalendarWindow::edit);connect(timeline,&Timeline::reorderRequested,this,&CalendarWindow::reorder);connect(timeline,&Timeline::scrollRequested,scroll,[scroll](int delta){auto bar=scroll->verticalScrollBar();bar->setValue(bar->value()-delta);});
 connect(timeline,&Timeline::taskToggled,this,[this](int index,bool value){if(index<0||index>=timeline->tasks.size())return;auto old=timeline->tasks;timeline->tasks[index].completed=value;for(auto &node:timeline->tasks[index].nodes)node.completed=value;if(!save())timeline->tasks=old;timeline->refresh();});
 connect(timeline,&Timeline::nodeToggled,this,[this](int task,int node,bool value){if(task<0||task>=timeline->tasks.size()||node<0||node>=timeline->tasks[task].nodes.size())return;auto old=timeline->tasks;timeline->tasks[task].nodes[node].completed=value;if(!save())timeline->tasks=old;timeline->refresh();});
 auto timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{timeline->refresh();if(writable)updateStatus();});timer->start(60000);
}
void CalendarWindow::updateStatus(){int remaining=0,complete=0;for(const auto &task:timeline->tasks){complete+=task.isComplete();for(const auto &node:task.nodes)remaining+=!node.completed;}status->setText(QStringLiteral("%1 个任务 · %2 完成 · %3 个待办节点").arg(timeline->tasks.size()).arg(complete).arg(remaining));}
bool CalendarWindow::load(){QFile file(dataPath);if(!file.exists()){timeline->tasks.clear();writable=true;updateStatus();return true;}QString error;QVector<TaskEntry> loaded;if(!file.open(QIODevice::ReadOnly)){error=file.errorString();}else TaskData::decode(file.readAll(),loaded,error);if(!error.isEmpty()){writable=false;timeline->tasks.clear();status->setText(QStringLiteral("读取失败，禁止覆盖：")+error);status->setToolTip(dataPath);return false;}timeline->tasks=loaded;writable=true;updateStatus();return true;}
bool CalendarWindow::save(){if(!writable){QMessageBox::warning(this,QStringLiteral("不能保存"),QStringLiteral("原任务文件读取失败，请先修复：\n")+dataPath);return false;}QDir().mkpath(QFileInfo(dataPath).absolutePath());QSaveFile file(dataPath);auto bytes=TaskData::encode(timeline->tasks);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){QMessageBox::warning(this,QStringLiteral("保存失败"),file.errorString());return false;}updateStatus();status->setToolTip(QStringLiteral("已保存至 ")+dataPath);return true;}
void CalendarWindow::reorder(int from,int to){if(from<0||to<0||from>=timeline->tasks.size()||to>=timeline->tasks.size()||from==to)return;auto old=timeline->tasks;timeline->tasks.move(from,to);if(!save())timeline->tasks=old;else timeline->selected=to;timeline->refresh();}
void CalendarWindow::edit(int index){
 TaskEntry task;if(index>=0)task=timeline->tasks[index];else task.id=uuid();QDialog dialog(this);dialog.setObjectName("taskEditor");dialog.setWindowTitle(index<0?QStringLiteral("新建任务"):QStringLiteral("编辑任务"));dialog.resize(620,460);dialog.setStyleSheet("QWidget{background:#faf9f5;color:#293f3c;font-size:12px} QLineEdit,QTextEdit,QTableWidget,QCalendarWidget{background:white;color:#293f3c;border:1px solid #cdd5c9} QPushButton,QToolButton{background:#fff;color:#293f3c;border:1px solid #cdd5c9;padding:5px 8px} QHeaderView::section{background:#edf0e8;padding:5px;border:0} QTableWidget{selection-background-color:#dbe6df}");auto root=new QVBoxLayout(&dialog);auto form=new QFormLayout;QLineEdit name(task.name);name.setObjectName("taskName");name.setClearButtonEnabled(true);form->addRow(QStringLiteral("任务名称 *"),&name);root->addLayout(form);
 auto table=new QTableWidget(0,4,&dialog);table->setObjectName("nodesTable");table->setHorizontalHeaderLabels({QStringLiteral("完成"),QStringLiteral("节点名称"),QStringLiteral("日期（可留空）"),QStringLiteral("删除")});table->verticalHeader()->hide();table->setSelectionBehavior(QAbstractItemView::SelectRows);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);table->setColumnWidth(0,42);table->setColumnWidth(2,178);table->setColumnWidth(3,42);root->addWidget(table,1);
 auto append=[&](const TaskNode &node){int row=table->rowCount();table->insertRow(row);table->setRowHeight(row,32);auto done=new QTableWidgetItem;done->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsSelectable);done->setCheckState(node.completed?Qt::Checked:Qt::Unchecked);table->setItem(row,0,done);auto title=new QTableWidgetItem(node.name);title->setData(Qt::UserRole,node.id.isEmpty()?uuid():node.id);table->setItem(row,1,title);
  auto dateBox=new QWidget(table);auto layout=new QHBoxLayout(dateBox);layout->setContentsMargins(2,1,2,1);layout->setSpacing(2);auto date=new QLineEdit(node.date.toString(Qt::ISODate),dateBox);date->setObjectName("nodeDate");date->setPlaceholderText("YYYY-MM-DD");date->setClearButtonEnabled(true);layout->addWidget(date);auto calendar=new QToolButton(dateBox);calendar->setText("…");calendar->setToolTip(QStringLiteral("选择日期"));calendar->setFixedWidth(25);layout->addWidget(calendar);table->setCellWidget(row,2,dateBox);
  connect(calendar,&QToolButton::clicked,&dialog,[&,date]{QDialog popup(&dialog);popup.setWindowTitle(QStringLiteral("选择节点日期"));auto layout=new QVBoxLayout(&popup);auto picker=new QCalendarWidget(&popup);layout->addWidget(picker);QDate seed=TaskData::parseDate(date->text());picker->setSelectedDate(seed.isValid()?seed:QDate::currentDate());connect(picker,&QCalendarWidget::clicked,&popup,[&](QDate d){date->setText(d.toString(Qt::ISODate));popup.accept();});popup.exec();});
  auto remove=new QToolButton(table);remove->setText("×");remove->setToolTip(QStringLiteral("移除此节点，保存任务后生效"));table->setCellWidget(row,3,remove);connect(remove,&QToolButton::clicked,&dialog,[table,remove]{for(int r=0;r<table->rowCount();r++)if(table->cellWidget(r,3)==remove){table->removeRow(r);break;}});
 };
 for(const auto &node:task.nodes)append(node);auto addNode=new QPushButton(QStringLiteral("＋ 节点"),&dialog);addNode->setObjectName("addNodeButton");root->addWidget(addNode,0,Qt::AlignLeft);connect(addNode,&QPushButton::clicked,&dialog,[&,append]{TaskNode node;append(node);int row=table->rowCount()-1;table->scrollToItem(table->item(row,1));table->editItem(table->item(row,1));});
 QTextEdit note(task.note);note.setObjectName("taskNote");note.setAcceptRichText(false);note.setPlaceholderText(QStringLiteral("任务备注（可选）"));note.setMaximumHeight(70);root->addWidget(&note);QLabel error;error.setWordWrap(true);error.setStyleSheet("color:#b64e38");root->addWidget(&error);QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);root->addWidget(&buttons);
 if(index>=0){auto remove=buttons.addButton(QStringLiteral("删除任务"),QDialogButtonBox::DestructiveRole);connect(remove,&QPushButton::clicked,&dialog,[&]{if(QMessageBox::question(&dialog,QStringLiteral("删除任务"),QStringLiteral("确定删除 %1 及其全部节点？").arg(task.name))!=QMessageBox::Yes)return;auto old=timeline->tasks;timeline->tasks.removeAt(index);if(!save()){timeline->tasks=old;return;}timeline->selected=-1;timeline->refresh();dialog.reject();});}
 for(auto button:buttons.findChildren<QPushButton*>()){button->setAutoDefault(false);button->setDefault(false);}connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(name.text().trimmed().isEmpty()){error.setText(QStringLiteral("请填写任务名称。"));name.setFocus();return;}QVector<TaskNode> nodes;
  for(int row=0;row<table->rowCount();row++){QString nodeName=table->item(row,1)->text().trimmed();auto date=table->cellWidget(row,2)->findChild<QLineEdit*>("nodeDate");QString text=date->text().trimmed();if(nodeName.isEmpty()&&text.isEmpty())continue;if(nodeName.isEmpty()){error.setText(QStringLiteral("第 %1 个节点缺少名称。").arg(row+1));table->setCurrentCell(row,1);return;}QDate parsed=TaskData::parseDate(text);if(!text.isEmpty()&&!parsed.isValid()){error.setText(QStringLiteral("第 %1 个节点日期无效，例如 2026-11-15。").arg(row+1));date->setFocus();return;}TaskNode node;node.id=table->item(row,1)->data(Qt::UserRole).toString();node.name=nodeName;node.date=parsed;node.completed=table->item(row,0)->checkState()==Qt::Checked;nodes.append(node);}
  task.name=name.text().trimmed();task.note=note.toPlainText();task.nodes=nodes;dialog.accept();});
 if(dialog.exec()!=QDialog::Accepted)return;auto old=timeline->tasks;if(index<0){timeline->tasks.append(task);index=timeline->tasks.size()-1;}else timeline->tasks[index]=task;if(!save()){timeline->tasks=old;return;}timeline->selected=index;timeline->refresh();
}

void CalendarWindow::setLocked(bool value) {
  locked = value;
  QSignalBlocker blocker(lockButton);
  lockButton->setChecked(value);
  lockButton->setIcon(lockIcon(value));
  lockButton->setAccessibleName(value?QStringLiteral("已锁定，点击解锁"):QStringLiteral("未锁定，点击锁定"));
  lockButton->setToolTip(value
                             ? QStringLiteral("窗口位置和大小已锁定，点击解锁")
                             : QStringLiteral("点击锁定窗口位置和大小"));
  settings.setValue("locked", value);
}
bool CalendarWindow::eventFilter(QObject *object, QEvent *event) {
  if (object->objectName() == "dragHeader" &&
      event->type() == QEvent::MouseButtonPress && !locked) {
    auto mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() == Qt::LeftButton && windowHandle()) {
      windowHandle()->startSystemMove();
      return true;
    }
  }
  return QWidget::eventFilter(object, event);
}
#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool CalendarWindow::nativeEvent(const QByteArray &, void *message,
                                 qintptr *result) {
#else
bool CalendarWindow::nativeEvent(const QByteArray &, void *message,
                                 long *result) {
#endif
  auto msg = static_cast<MSG *>(message);
  if (msg->message != WM_NCHITTEST)
    return false;
  if (locked) {
    *result = HTCLIENT;
    return true;
  }
  RECT r;
  GetWindowRect(reinterpret_cast<HWND>(winId()), &r);
  int px = GET_X_LPARAM(msg->lParam), py = GET_Y_LPARAM(msg->lParam),
      border = qRound(5 * devicePixelRatioF());
  bool left = px < r.left + border, right = px >= r.right - border,
       top = py < r.top + border, bottom = py >= r.bottom - border;
  *result = top      ? (left    ? HTTOPLEFT
                        : right ? HTTOPRIGHT
                                : HTTOP)
            : bottom ? (left    ? HTBOTTOMLEFT
                        : right ? HTBOTTOMRIGHT
                                : HTBOTTOM)
            : left   ? HTLEFT
            : right  ? HTRIGHT
                     : HTCLIENT;
  return true;
}
#endif

void CalendarWindow::closeEvent(QCloseEvent *event){settings.setValue("geometry",saveGeometry());settings.setValue("days",timeline->days);QWidget::closeEvent(event);}
void CalendarWindow::paintEvent(QPaintEvent*){QPainter p(this);p.fillRect(rect(),QColor(48,48,48,128));}
