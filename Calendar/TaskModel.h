#pragma once
#include <QtWidgets>
struct TaskNode {
  QString id, name;
  QDate date;
  bool completed=false;
  QJsonObject json() const;
};
struct TaskEntry {
  QString id,name,note;
  QVector<TaskNode> nodes;
  bool completed=false;
  int completedNodes() const;
  bool isComplete() const;
  QJsonObject json() const;
};
namespace TaskData {
QDate parseDate(QString text);
QString countdown(QDate date,QDate today=QDate::currentDate(),bool completed=false);
QByteArray encode(const QVector<TaskEntry>& tasks);
bool decode(const QByteArray&,QVector<TaskEntry>&,QString& error);
}
