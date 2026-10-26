#include "TaskModel.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
QJsonObject TaskNode::json() const {return {{"id",id},{"name",name},{"date",date.isValid()?QJsonValue(date.toString(Qt::ISODate)):QJsonValue()},{"completed",completed}};}
int TaskEntry::completedNodes() const {int count=0;for(const auto &node:nodes)count+=node.completed;return count;}
bool TaskEntry::isComplete() const {return nodes.isEmpty()?completed:completedNodes()==nodes.size();}
QJsonObject TaskEntry::json() const {QJsonArray list;for(const auto &node:nodes)list.append(node.json());return {{"id",id},{"name",name},{"note",note},{"completed",isComplete()},{"nodes",list}};}
QDate TaskData::parseDate(QString text){text=text.trimmed();auto match=QRegularExpression("^(\\d{4})[-/](\\d{1,2})[-/](\\d{1,2})$").match(text);if(match.hasMatch())return QDate(match.captured(1).toInt(),match.captured(2).toInt(),match.captured(3).toInt());return text.size()==8?QDate::fromString(text,"yyyyMMdd"):QDate();}
QString TaskData::countdown(QDate date,QDate today,bool completed){if(completed)return QStringLiteral("已完成");if(!date.isValid())return QStringLiteral("日期待定");qint64 delta=today.daysTo(date);return delta>0?QStringLiteral("还有 %1 天").arg(delta):delta<0?QStringLiteral("已过 %1 天").arg(-delta):QStringLiteral("今天");}
QByteArray TaskData::encode(const QVector<TaskEntry> &tasks){QJsonArray list;for(const auto &task:tasks)list.append(task.json());return QJsonDocument(QJsonObject{{"version",1},{"tasks",list}}).toJson();}
bool TaskData::decode(const QByteArray &bytes,QVector<TaskEntry> &result,QString &error){
 QJsonParseError parse;auto document=QJsonDocument::fromJson(bytes,&parse);result.clear();
 if(parse.error!=QJsonParseError::NoError||!document.isObject()||document.object()["version"].toInt()!=1||!document.object()["tasks"].isArray()){error=QStringLiteral("任务数据格式或版本无效");return false;}
 QVector<TaskEntry> decoded;QSet<QString> taskIds;
 for(auto value:document.object()["tasks"].toArray()){
  auto object=value.toObject();TaskEntry task;task.id=object["id"].toString();task.name=object["name"].toString();task.note=object["note"].toString();task.completed=object["completed"].toBool();
  if(!value.isObject()||task.id.isEmpty()||taskIds.contains(task.id)||task.name.trimmed().isEmpty()||!object["nodes"].isArray()){error=QStringLiteral("任务名称、标识或节点列表无效");return false;}taskIds.insert(task.id);QSet<QString> nodeIds;
  for(auto nodeValue:object["nodes"].toArray()){
   auto nodeObject=nodeValue.toObject();TaskNode node;node.id=nodeObject["id"].toString();node.name=nodeObject["name"].toString();node.completed=nodeObject["completed"].toBool();auto date=nodeObject["date"];node.date=QDate::fromString(date.toString(),Qt::ISODate);
   if(!nodeValue.isObject()||node.id.isEmpty()||nodeIds.contains(node.id)||node.name.trimmed().isEmpty()||(!date.isNull()&&(!date.isString()||!node.date.isValid()))){error=QStringLiteral("任务“%1”的节点记录无效").arg(task.name);return false;}nodeIds.insert(node.id);task.nodes.append(node);
  }decoded.append(task);
 }
 result=decoded;error.clear();return true;
}
