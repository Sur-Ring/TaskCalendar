#include "CalendarWindow.h"
#include <algorithm>
#include <cmath>
namespace {
constexpr auto Mime="application/x-task-calendar-row";
QFont textFont(int size=10,bool bold=false){QFont f("Microsoft YaHei UI");f.setPixelSize(size);f.setBold(bold);return f;}
int dropY(const QDropEvent *event){
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
 return qRound(event->position().y());
#else
 return event->pos().y();
#endif
}
QString shortCount(const TaskNode &n){if(n.completed)return QStringLiteral("完成");auto d=QDate::currentDate().daysTo(n.date);return d>0?QStringLiteral("剩%1天").arg(d):d<0?QStringLiteral("过%1天").arg(-d):QStringLiteral("今天");}
}
QColor Timeline::tint(QColor c){return QColor(qRound(c.red()*.15+255*.85),qRound(c.green()*.15+255*.85),qRound(c.blue()*.15+255*.85));}
QColor Timeline::ink(QColor c){auto lum=[](QColor v){auto f=[](double x){x/=255;return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};return .2126*f(v.red())+.7152*f(v.green())+.0722*f(v.blue());};double bg=lum(tint(c));while((bg+.05)/(lum(c)+.05)<7)c=QColor(int(c.red()*.9),int(c.green()*.9),int(c.blue()*.9));return c;}
QVector<NodeLabel> Timeline::layoutLabels(const TaskEntry &task,QDate start,int days,double width){
 if(days<=0||width<40)return {};
 QVector<QPair<int,double>> points;for(int i=0;i<task.nodes.size();i++){if(!task.nodes[i].date.isValid())continue;double anchor=start.daysTo(task.nodes[i].date)*width/days;if(anchor>=0&&anchor<=width)points.append({i,anchor});}
 std::stable_sort(points.begin(),points.end(),[](auto a,auto b){return a.second<b.second;});
 QVector<NodeLabel> groups;double lastPoint=-100;
 for(auto point:points){if(groups.isEmpty()||point.second-lastPoint>=14){NodeLabel label;label.anchor=point.second;label.nodes.append(point.first);groups.append(label);}else{auto &label=groups.last();label.anchor=(label.anchor*label.nodes.size()+point.second)/(label.nodes.size()+1);label.nodes.append(point.first);}lastPoint=point.second;}
 QFontMetricsF titleMetrics(textFont(10,true)),detailMetrics(textFont());
 while(!groups.isEmpty()){
  QVector<bool> compact(groups.size(),false);QVector<QRectF> natural;
  for(auto &label:groups){const auto &first=task.nodes[label.nodes[0]],&last=task.nodes[label.nodes.last()];
   label.title=label.nodes.size()==1?first.name:QStringLiteral("%1 个节点").arg(label.nodes.size());
   label.subtitle=label.nodes.size()==1?first.date.toString("MM/dd")+" · "+shortCount(first):first.date==last.date?first.date.toString("MM/dd"):first.date.toString("MM/dd")+"–"+last.date.toString("MM/dd");
   double w=qMin(width-6,qMax(72.0,qMax(titleMetrics.horizontalAdvance(label.title),detailMetrics.horizontalAdvance(label.subtitle))+10));
   natural.append(QRectF(qBound(3.0,label.anchor-w/2,width-3-w),9,w,27));
  }
  for(int i=0;i<groups.size();i++)for(int j=i+1;j<groups.size();j++)if(natural[i].adjusted(-2,0,2,0).intersects(natural[j].adjusted(-2,0,2,0))){compact[i]=compact[j]=true;}
  double right[2]={-1,-1};bool packed=true;int failed=0;
  for(int i=0;i<groups.size();i++){auto &label=groups[i];double w=natural[i].width(),h=27;
   if(compact[i]){label.subtitle.clear();w=qMin(width-6,qMax(36.0,titleMetrics.horizontalAdvance(label.title)+10));h=18;}
   double desired=qBound(3.0,label.anchor-w/2,width-3-w),bestLeft=0,bestCost=1e100;int lane=-1;
   for(int j=0;j<(compact[i]?2:1);j++){double left=qMax(desired,right[j]+4);if(!compact[i])left=qMax(left,right[1]+4);double cost=qAbs(left+w/2-label.anchor)+j*.25;if(left+w<=width-3&&cost<bestCost){lane=j;bestLeft=left;bestCost=cost;}}
   if(lane<0){packed=false;failed=i;break;}label.rect=QRectF(bestLeft,9+22*lane,w,h);right[lane]=bestLeft+w;
   // Full-height labels occupy both lanes.
   if(!compact[i])right[1]=qMax(right[1],bestLeft+w);
  }
  if(packed)return groups;
  int pair=0;double distance=1e100;for(int i=0;i<failed;i++){double d=groups[i+1].anchor-groups[i].anchor;if(d<distance){distance=d;pair=i;}}
  auto &left=groups[pair];auto rightGroup=groups[pair+1];left.anchor=(left.anchor*left.nodes.size()+rightGroup.anchor*rightGroup.nodes.size())/(left.nodes.size()+rightGroup.nodes.size());left.nodes+=rightGroup.nodes;groups.removeAt(pair+1);
 }

 return groups;
}
Timeline::Timeline(QWidget *parent):QWidget(parent){
 holdTimer.setInterval(16);connect(&holdTimer,&QTimer::timeout,this,[this]{
  if(holdRow<0||holdRow>=tasks.size()||tasks[holdRow].id!=holdTaskId||holdNode<0||holdNode>=tasks[holdRow].nodes.size()||tasks[holdRow].nodes[holdNode].id!=holdNodeId){cancelHold();return;}
  if(holdElapsed.elapsed()>=500){int row=holdRow,node=holdNode;bool completed=!tasks[row].nodes[node].completed;holdConsumed=true;cancelHold();QToolTip::hideText();emit nodeToggled(row,node,completed);refresh();}else update();
 });
 setMinimumWidth(360);setMouseTracking(true);setAcceptDrops(true);setFocusPolicy(Qt::StrongFocus);
 todayOverflow=new QPushButton(this);todayOverflow->setObjectName("todayOverflow");todayOverflow->setStyleSheet("QPushButton{font-size:10px;color:#ffa58e;background:rgba(48,48,48,210);border:0;padding:0 3px}");todayOverflow->setToolTip(QStringLiteral("回到今天"));todayOverflow->hide();connect(todayOverflow,&QPushButton::clicked,this,[this]{start=QDate::currentDate().addDays(-days/4);refresh();});refresh();
}
void Timeline::cancelHold(){holdTimer.stop();holdRow=holdNode=-1;update();}
void Timeline::refresh(){cancelHold();rows.clear();int top=AxisHeight;for(const auto &task:tasks){TaskRow row;row.top=top;row.labels=layoutLabels(task,start,days,qMax(40,width()-NameWidth));for(const auto &label:row.labels)row.height=qMax(row.height,int(label.rect.bottom())+4);top+=row.height;rows.append(row);}setMinimumHeight(qMax(AxisHeight+42,top));update();}
void Timeline::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);refresh();}
int Timeline::rowTop(int i) const{return i>=0&&i<rows.size()?rows[i].top:-1;}
int Timeline::rowHeight(int i) const{return i>=0&&i<rows.size()?rows[i].height:0;}
void Timeline::setScrollOffset(int value){cancelHold();headerOffset=qMax(0,value);update();}
int Timeline::rowAt(int y) const{if(y<headerOffset+AxisHeight)return -1;for(int i=0;i<rows.size();i++)if(y>=rows[i].top&&y<rows[i].top+rows[i].height)return i;return -1;}
int Timeline::insertionAt(int y) const{for(int i=0;i<rows.size();i++)if(y<rows[i].top+rows[i].height/2)return i;return rows.size();}
void Timeline::paintEvent(QPaintEvent*){
 QPainter p(this);p.setRenderHint(QPainter::Antialiasing);double tw=width()-NameWidth,scale=tw/days;auto x=[&](QDate d){return NameWidth+start.daysTo(d)*scale;};double today=x(QDate::currentDate());int tick=days<=45?7:days<=120?14:days<=240?30:60;while(tick*scale<48)tick*=2;
 p.save();p.setClipRect(QRect(0,headerOffset+AxisHeight,width(),height()),Qt::IntersectClip);
 for(int n=0;n<days;n+=tick){double px=x(start.addDays(n));p.setPen(QColor(255,255,255,30));p.drawLine(QPointF(px,AxisHeight),QPointF(px,height()));}
 for(int i=0;i<rows.size();i++){
  const auto &row=rows[i];const auto &task=tasks[i];int y=row.top,h=row.height;
  if(i==selected)p.fillRect(QRect(0,y,NameWidth,h),QColor(255,255,255,10));p.setPen(QColor(255,255,255,30));p.drawLine(0,y,width(),y);p.drawLine(NameWidth,y,NameWidth,y+h);
  p.setPen(Qt::NoPen);p.setBrush(QColor("#a2ada5"));for(int dy:{-5,0,5})for(int dx:{6,11})p.drawEllipse(QPointF(dx,y+h/2+dy),1,1);
  auto nameFont=textFont(12,true);nameFont.setStrikeOut(task.isComplete());p.setFont(nameFont);p.setPen(task.isComplete()?QColor("#a4afa8"):QColor("#eef2ef"));p.drawText(QRect(21,y+(h-34)/2,NameWidth-25,18),Qt::AlignVCenter,p.fontMetrics().elidedText(task.name,Qt::ElideRight,NameWidth-26));
  int undated=0;for(const auto &node:task.nodes)undated+=!node.date.isValid();p.setFont(textFont(9));p.setPen(QColor("#b2bdb5"));p.drawText(QRect(21,y+(h-34)/2+19,NameWidth-25,14),Qt::AlignVCenter,task.nodes.isEmpty()?(task.isComplete()?QStringLiteral("已完成"):QStringLiteral("尚无节点")):QString("%1/%2").arg(task.completedNodes()).arg(task.nodes.size())+(undated?QStringLiteral(" · %1待定").arg(undated):QString()));
  p.save();p.setClipRect(QRect(NameWidth,y,int(tw),h),Qt::IntersectClip);
  double first=width(),last=NameWidth;bool any=false;
  for(const auto &node:task.nodes)if(node.date.isValid()){first=qMin(first,x(node.date));last=qMax(last,x(node.date));any=true;}
  if(any&&last>first)p.fillRect(QRectF(first,y,last-first,h),QColor(255,255,255,7));
  if(today>=NameWidth&&today<=width()){p.setPen(QPen(QColor("#d66147"),1.5));p.drawLine(QPointF(today,y),QPointF(today,y+h));}
  for(const auto &node:task.nodes){if(!node.date.isValid())continue;double px=x(node.date);if(px<NameWidth||px>width())continue;p.setPen(QPen(QColor("#91b5b0"),1));p.drawLine(QPointF(px,y),QPointF(px,y+h));p.setBrush(QColor("#91b5b0"));p.setPen(QColor("#faf9f5"));p.drawEllipse(QPointF(px,y+3.5),2.5,2.5);}
  for(const auto &label:row.labels){const auto &node=task.nodes[label.nodes[0]];auto r=label.rect.translated(NameWidth,y);p.setPen(QPen(QColor("#91b5b0"),1));p.drawLine(QPointF(NameWidth+label.anchor,y+4),QPointF(r.center().x(),r.top()));p.setBrush(tint(QColor("#91b5b0")));p.drawRoundedRect(r,3,3);p.setPen(ink(QColor("#91b5b0")));auto f=textFont(10,true);bool done=true;for(int index:label.nodes)done&=task.nodes[index].completed;f.setStrikeOut(done);p.setFont(f);if(label.subtitle.isEmpty())p.drawText(r.adjusted(4,0,-4,0),Qt::AlignCenter,label.title);else{p.drawText(r.adjusted(4,0,-4,-13),Qt::AlignCenter,label.title);p.setFont(textFont());p.drawText(r.adjusted(3,13,-3,0),Qt::AlignCenter,label.subtitle);}}
  if(!any){p.setFont(textFont());p.setPen(QColor("#b2bdb5"));p.drawText(QRect(NameWidth+10,y,int(tw)-20,h),Qt::AlignVCenter,task.nodes.isEmpty()?QStringLiteral("点击任务名称添加节点"):QStringLiteral("节点日期待定，点击任务名称编辑"));}p.restore();
 }
 if(insertion>=0){p.setPen(QPen(QColor("#eef2ef"),2));int y=insertion<rows.size()?rows[insertion].top:rows.isEmpty()?AxisHeight:rows.last().top+rows.last().height;p.drawLine(0,y,width(),y);}
 if(tasks.isEmpty()){p.setPen(QColor("#d3d9d5"));p.drawText(QRect(0,AxisHeight,width(),42),Qt::AlignCenter,QStringLiteral("点击「＋ 任务」开始"));}
 if(holdTimer.isActive()){
  QPointF center(qBound(holdRect.left()+10.0,double(press.x()),holdRect.right()-10.0),holdRect.center().y());QRectF ring(center.x()-8,center.y()-8,16,16);
  p.setBrush(QColor("#edf4f1"));p.setPen(QPen(QColor("#bacac5"),2));p.drawEllipse(ring);p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#295e56"),2.5,Qt::SolidLine,Qt::RoundCap));p.drawArc(ring,90*16,-qRound(qMin(1.0,holdElapsed.elapsed()/500.0)*360*16));
 }
 p.restore();p.save();p.translate(0,headerOffset);
 p.setFont(textFont());p.setPen(QColor("#d3d9d5"));p.drawText(QRect(10,8,120,20),Qt::AlignVCenter,QStringLiteral("任务 / 节点进度"));
 for(int n=0;n<days;n+=tick){double px=x(start.addDays(n));p.setPen(QColor(255,255,255,30));p.setPen(QColor("#c6cec7"));p.drawText(QRectF(qBound(double(NameWidth),px-21,double(width()-43)),14,42,17),Qt::AlignCenter,start.addDays(n).toString("MM/dd"));}
 bool outside=today<NameWidth||today>width();todayOverflow->setVisible(outside);if(outside){bool left=today<NameWidth;todayOverflow->setText(left?QStringLiteral("← 今天 %1").arg(QDate::currentDate().toString("MM/dd")):QStringLiteral("今天 %1 →").arg(QDate::currentDate().toString("MM/dd")));todayOverflow->adjustSize();todayOverflow->resize(todayOverflow->width(),15);todayOverflow->move(left?NameWidth+3:width()-todayOverflow->width()-3,headerOffset);todayOverflow->raise();}else{p.setFont(textFont());p.setPen(QColor("#d66147"));p.drawLine(QPointF(today,0),QPointF(today,AxisHeight));p.drawText(QRectF(qBound(double(NameWidth),today+4,double(width()-68)),0,66,15),Qt::AlignVCenter,QStringLiteral("今天 ")+QDate::currentDate().toString("MM/dd"));}
 p.setPen(QColor(255,255,255,50));p.drawLine(0,AxisHeight-1,width(),AxisHeight-1);p.restore();

}
QVector<int> Timeline::nodesAt(QPoint point) const{int row=rowAt(point.y());if(row<0||point.x()<NameWidth)return {};const auto &layout=rows[row];for(const auto &label:layout.labels)if(label.rect.translated(NameWidth,layout.top).contains(point))return label.nodes;QVector<int> found;double tw=width()-NameWidth;for(int i=0;i<tasks[row].nodes.size();i++){const auto &node=tasks[row].nodes[i];if(node.date.isValid()&&std::abs(NameWidth+start.daysTo(node.date)*tw/days-point.x())<5)found.append(i);}return found;}
void Timeline::showNodes(int taskIndex,const QVector<int> &indexes,QPoint position){
 QToolTip::hideText();auto menu=new QMenu(this);menu->setObjectName("nodeDetails");menu->setAttribute(Qt::WA_DeleteOnClose);menu->setStyleSheet("QMenu,QWidget{background:#faf9f5;color:#293f3c} QCheckBox{font-size:12px} QLabel{font-size:11px} QPushButton{background:white;border:1px solid #cdd5c9;padding:6px}");auto box=new QWidget(menu);auto layout=new QVBoxLayout(box);layout->setContentsMargins(12,10,12,10);auto title=new QLabel(tasks[taskIndex].name);title->setTextFormat(Qt::PlainText);title->setStyleSheet("font-weight:600");layout->addWidget(title);
 auto area=new QScrollArea;area->setWidgetResizable(true);area->setFrameShape(QFrame::NoFrame);auto list=new QWidget;auto listLayout=new QVBoxLayout(list);listLayout->setContentsMargins(0,0,0,0);
 for(int index:indexes){const auto &node=tasks[taskIndex].nodes[index];auto check=new QCheckBox(node.name);check->setChecked(node.completed);auto details=new QLabel((node.date.isValid()?node.date.toString(Qt::ISODate):QStringLiteral("日期待定"))+" · "+TaskData::countdown(node.date,QDate::currentDate(),node.completed));details->setTextFormat(Qt::PlainText);listLayout->addWidget(check);listLayout->addWidget(details);connect(check,&QCheckBox::toggled,this,[this,check,details,taskIndex,index](bool value){emit nodeToggled(taskIndex,index,value);const auto &updated=tasks[taskIndex].nodes[index];QSignalBlocker blocker(check);check->setChecked(updated.completed);details->setText((updated.date.isValid()?updated.date.toString(Qt::ISODate):QStringLiteral("日期待定"))+" · "+TaskData::countdown(updated.date,QDate::currentDate(),updated.completed));});}
 area->setWidget(list);area->setFixedWidth(300);area->setFixedHeight(qMin(300,qMax(55,indexes.size()*52)));layout->addWidget(area);auto edit=new QPushButton(QStringLiteral("编辑任务 / 全部节点"));layout->addWidget(edit);connect(edit,&QPushButton::clicked,this,[this,menu,taskIndex]{menu->close();QTimer::singleShot(0,this,[this,taskIndex]{emit editRequested(taskIndex);});});auto action=new QWidgetAction(menu);action->setDefaultWidget(box);menu->addAction(action);menu->popup(position);
}
void Timeline::mousePressEvent(QMouseEvent *e){if(e->button()!=Qt::LeftButton){cancelHold();return;}cancelHold();holdConsumed=false;press=e->pos();pressedRow=rowAt(press.y());selected=pressedRow;setFocus();
 if(pressedRow>=0&&press.x()>=NameWidth){for(const auto &label:rows[pressedRow].labels){auto rect=label.rect.translated(NameWidth,rows[pressedRow].top);if(label.nodes.size()==1&&rect.contains(press)){holdRow=pressedRow;holdNode=label.nodes[0];holdTaskId=tasks[holdRow].id;holdNodeId=tasks[holdRow].nodes[holdNode].id;holdRect=rect;holdElapsed.start();holdTimer.start();break;}}}update();}
void Timeline::mouseReleaseEvent(QMouseEvent *e){cancelHold();if(holdConsumed){holdConsumed=false;pressedRow=-1;e->accept();return;}int row=rowAt(e->pos().y());if(e->button()==Qt::LeftButton&&row>=0&&row==pressedRow&&(e->pos()-press).manhattanLength()<QApplication::startDragDistance()){
 if(press.x()>=18&&e->pos().x()<NameWidth)emit editRequested(row);else if(press.x()>=NameWidth){auto nodes=nodesAt(e->pos());if(!nodes.isEmpty())showNodes(row,nodes,mapToGlobal(e->pos()+QPoint(0,8)));}}
 pressedRow=-1;
}
void Timeline::mouseMoveEvent(QMouseEvent *e){if(holdTimer.isActive()&&!holdRect.contains(e->pos())){cancelHold();holdConsumed=true;}setCursor(e->pos().x()<18?Qt::OpenHandCursor:e->pos().x()<NameWidth||!nodesAt(e->pos()).isEmpty()?Qt::PointingHandCursor:Qt::ArrowCursor);if((e->buttons()&Qt::LeftButton)&&pressedRow>=0&&press.x()<18&&(e->pos()-press).manhattanLength()>=QApplication::startDragDistance()){auto drag=new QDrag(this);auto mime=new QMimeData;mime->setData(Mime,tasks[pressedRow].id.toUtf8());drag->setMimeData(mime);drag->exec(Qt::MoveAction);drag->deleteLater();pressedRow=-1;insertion=-1;update();}}
void Timeline::wheelEvent(QWheelEvent *e){if(holdTimer.isActive())holdConsumed=true;cancelHold();int delta=e->angleDelta().y()?e->angleDelta().y():e->pixelDelta().y();if(!delta){e->ignore();return;}
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
 double wheelX=e->position().x();
#else
 double wheelX=e->posF().x();
#endif
 if(wheelX<NameWidth)emit scrollRequested(delta);else if(e->modifiers()&Qt::ControlModifier){QDate center=start.addDays(days/2);days=qBound(14,qRound(days*(delta>0?.8:1.25)),730);start=center.addDays(-days/2);}else start=start.addDays((delta>0?-1:1)*qMax(1,qRound(days/20.0)));refresh();e->accept();}
void Timeline::keyPressEvent(QKeyEvent *e){if((e->modifiers()&Qt::AltModifier)&&selected>=0&&(e->key()==Qt::Key_Up||e->key()==Qt::Key_Down)){int to=selected+(e->key()==Qt::Key_Up?-1:1);if(to>=0&&to<tasks.size())emit reorderRequested(selected,to);e->accept();return;}QWidget::keyPressEvent(e);}
bool Timeline::validDrag(const QMimeData *mime) const{if(!mime->hasFormat(Mime))return false;QString id=QString::fromUtf8(mime->data(Mime));for(const auto &task:tasks)if(task.id==id)return true;return false;}
void Timeline::dragEnterEvent(QDragEnterEvent *e){if(e->source()==this&&validDrag(e->mimeData()))e->acceptProposedAction();}
void Timeline::dragMoveEvent(QDragMoveEvent *e){if(e->source()!=this||!validDrag(e->mimeData()))return;insertion=insertionAt(dropY(e));update();e->acceptProposedAction();}
void Timeline::dragLeaveEvent(QDragLeaveEvent *e){insertion=-1;update();e->accept();}
void Timeline::dropEvent(QDropEvent *e){if(e->source()!=this||!validDrag(e->mimeData()))return;QString id=QString::fromUtf8(e->mimeData()->data(Mime));int from=-1;for(int i=0;i<tasks.size();i++)if(tasks[i].id==id)from=i;int to=insertionAt(dropY(e));if(to>from)--to;insertion=-1;if(from!=to)emit reorderRequested(from,to);update();e->acceptProposedAction();}
bool Timeline::event(QEvent *event){if(event->type()==QEvent::Leave||event->type()==QEvent::WindowDeactivate||event->type()==QEvent::Hide){if(holdTimer.isActive())holdConsumed=true;cancelHold();}if(event->type()==QEvent::ToolTip&&holdTimer.isActive())return true;if(event->type()==QEvent::ToolTip){auto e=static_cast<QHelpEvent*>(event);int row=rowAt(e->pos().y());if(row>=0){QString text=tasks[row].name;auto nodes=nodesAt(e->pos());if(!nodes.isEmpty()){QStringList lines;for(int index:nodes){const auto &node=tasks[row].nodes[index];lines<<node.name+" · "+node.date.toString(Qt::ISODate)+" · "+TaskData::countdown(node.date,QDate::currentDate(),node.completed)+QStringLiteral("（长按标签 0.5 秒切换完成）");if(lines.size()==6){if(nodes.size()>6)lines<<QStringLiteral("另有 %1 项，点击展开").arg(nodes.size()-6);break;}}text=lines.join('\n');}QToolTip::showText(e->globalPos(),text,this);return true;}}return QWidget::event(event);}



