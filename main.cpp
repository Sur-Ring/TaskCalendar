#include "Calendar/CalendarWindow.h"
#include <QApplication>
#include <QSaveFile>
#include <memory>
int main(int argc,char *argv[]){
 QApplication app(argc,argv);QCoreApplication::setOrganizationName("TaskCalendar");QCoreApplication::setApplicationName("TaskCalendar");
 auto args=app.arguments();int dataIndex=args.indexOf("--data-dir");QString directory=dataIndex>=0&&dataIndex+1<args.size()?QDir(args[dataIndex+1]).absolutePath():QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(directory);QLockFile lock(directory+"/instance.lock");lock.setStaleLockTime(0);if(!lock.tryLock(0)){QMessageBox::information(nullptr,QStringLiteral("TaskCalendar 已在运行"),QStringLiteral("此数据目录的任务日历已经运行，请检查桌面。"));return 0;}
 CalendarWindow window;
 if(args.contains("--demo")){
  auto timeline=window.findChild<Timeline*>();timeline->tasks.clear();QStringList taskNames={QStringLiteral("毕业论文"),QStringLiteral("网站上线"),QStringLiteral("个人学习")};QVector<QStringList> names={{QStringLiteral("确定选题"),QStringLiteral("文献整理"),QStringLiteral("完成初稿"),QStringLiteral("修改校对"),QStringLiteral("最终提交")},{QStringLiteral("接口完成"),QStringLiteral("联调测试"),QStringLiteral("压力测试"),QStringLiteral("发布上线")},{QStringLiteral("第1章"),QStringLiteral("第2章"),QStringLiteral("第3章"),QStringLiteral("第4章"),QStringLiteral("第5章"),QStringLiteral("第6章"),QStringLiteral("第7章"),QStringLiteral("第8章"),QStringLiteral("总结复习")}};
  for(int i=0;i<3;i++){TaskEntry task;task.id=QUuid::createUuid().toString(QUuid::WithoutBraces);task.name=taskNames[i];task.note=QStringLiteral("演示任务，不会自动保存");for(int j=0;j<names[i].size();j++){TaskNode node;node.id=QUuid::createUuid().toString(QUuid::WithoutBraces);node.name=names[i][j];node.date=QDate::currentDate().addDays(i==0?-5+j*10:i==1?j+1:-7+j*8);node.completed=(i==0&&j==0)||(i==2&&j<2);task.nodes.append(node);}timeline->tasks.append(task);}timeline->refresh();if(auto status=window.findChild<QLabel*>("taskStatus"))status->setText(QStringLiteral("3 个演示任务 · 18 个自定义节点"));
 }
 window.show();if(args.contains("--smoke-test"))QTimer::singleShot(1000,&app,[&]{int snapshot=args.indexOf("--snapshot");bool ok=snapshot<0||snapshot+1>=args.size()||window.grab().save(args[snapshot+1]);app.exit(ok?0:2);});return app.exec();
}
