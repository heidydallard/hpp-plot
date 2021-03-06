#include "hpp/plot/hpp-manipulation-graph.hh"

#include <limits>

#include <QGVNode.h>
#include <QGVEdge.h>
#include <QMessageBox>
#include <QMap>
#include <QTemporaryFile>
#include <QDir>
#include <QMenu>
#include <QPushButton>
#include <QInputDialog>
#include <QTimer>
#include <QLayout>
#include <QDebug>
#include <QtGui/qtextdocument.h>

namespace hpp {
  namespace plot {
    namespace {
      void initConfigProjStat (::hpp::ConfigProjStat& p) {
        p.success = 0;
        p.error = 0;
        p.nbObs = 0;
      }
    }
    GraphAction::GraphAction(HppManipulationGraphWidget *parent) :
      QAction (parent),
      gw_ (parent)
    {
      connect (this, SIGNAL (triggered()), SLOT(transferSignal()));
    }

    void GraphAction::transferSignal()
    {
      hpp::ID id;
      if (gw_->selectionID(id))
        emit activated (id);
    }

    HppManipulationGraphWidget::HppManipulationGraphWidget (corbaServer::manipulation::Client* hpp_, QWidget *parent)
      : GraphWidget ("Manipulation graph", parent),
        manip_ (hpp_),
        updateStatsTimer_ (new QTimer (this)),
        currentId_ (-1)
    {
      QPushButton* stats = new QPushButton (
            QIcon::fromTheme("view-refresh"), "&Statistics", buttonBox_);
      stats->setCheckable(true);
      buttonBox_->layout()->addWidget(stats);
      updateStatsTimer_->setInterval(1000);
      updateStatsTimer_->setSingleShot(false);

      connect (updateStatsTimer_, SIGNAL (timeout()), SLOT (updateStatistics()));
      connect (stats, SIGNAL (clicked(bool)), SLOT (startStopUpdateStats(bool)));
      connect (scene_, SIGNAL (selectionChanged()), SLOT(selectionChanged()));
    }

    HppManipulationGraphWidget::~HppManipulationGraphWidget()
    {
      qDeleteAll (nodeContextMenuActions_);
      qDeleteAll (edgeContextMenuActions_);
      delete updateStatsTimer_;
    }

    void HppManipulationGraphWidget::addNodeContextMenuAction(GraphAction *action)
    {
      nodeContextMenuActions_.append (action);
    }

    void HppManipulationGraphWidget::addEdgeContextMenuAction(GraphAction *action)
    {
      edgeContextMenuActions_.append (action);
    }

    void HppManipulationGraphWidget::client (corbaServer::manipulation::Client* hpp)
    {
      manip_ = hpp;
    }

    bool hpp::plot::HppManipulationGraphWidget::selectionID(ID &id)
    {
      id = currentId_;
      return currentId_ != -1;
    }

    void HppManipulationGraphWidget::fillScene()
    {
      /*
      QTemporaryFile dotFile (QDir::tempPath () + QString ("/constraintGraph_XXXXXX.dot"));
      dotFile.setAutoRemove(false);
      qDebug() << dotFile.fileName();
      if (dotFile.open()) {
          manip_.graph()->display(dotFile.fileName().toLocal8Bit().constData());
          QByteArray content = dotFile.readAll();
          scene_->loadLayout(content.constData());
          qDebug() << dotFile.fileName();
        }
      //*/
//       /*
      hpp::GraphComp_var graph;
      hpp::GraphElements_var elmts;
      try {
        manip_->graph()->getGraph(graph.out(), elmts.out());

        scene_->setGraphAttribute("label", QString (graph->name));

        //      scene_->setGraphAttribute("splines", "ortho");
        scene_->setGraphAttribute("rankdir", "LR");
        //scene_->setGraphAttribute("concentrate", "true"); //Error !
        scene_->setGraphAttribute("nodesep", "0.4");

        scene_->setNodeAttribute("shape", "circle");
        scene_->setNodeAttribute("style", "filled");
        scene_->setNodeAttribute("fillcolor", "white");
        scene_->setNodeAttribute("height", "1.2");
        scene_->setEdgeAttribute("minlen", "3");
        //scene_->setEdgeAttribute("dir", "both");


        // Add the nodes
        QMap < ::CORBA::Long, QGVNode*> nodes;
        for (std::size_t i = 0; i < elmts->nodes.length(); ++i) {
          QGVNode* n = scene_->addNode (QString (elmts->nodes[i].name));
          NodeInfo ni;
          ni.id = elmts->nodes[i].id;
          nodeInfos_[n] = ni;
          n->setFlag (QGraphicsItem::ItemIsMovable, true);
          n->setFlag (QGraphicsItem::ItemSendsGeometryChanges, true);
          nodes[elmts->nodes[i].id] = n;
        }
        for (std::size_t i = 0; i < elmts->edges.length(); ++i) {
          QGVEdge* e = scene_->addEdge (nodes[elmts->edges[i].start],
              nodes[elmts->edges[i].end], "");
          EdgeInfo ei;
          ei.name = QString::fromLocal8Bit(elmts->edges[i].name);
          ei.id = elmts->edges[i].id;
          ei.edge = e;
          edgeInfos_[e] = ei;
          updateWeight (ei);
        }
      } catch (const hpp::Error& e) {
        qDebug () << e.msg;
      }
      //*/
    }

    void HppManipulationGraphWidget::updateStatistics()
    {
      foreach (QGraphicsItem* elmt, scene_->items()) {
          QGVNode* node = dynamic_cast <QGVNode*> (elmt);
          if (node) {
              NodeInfo& ni = nodeInfos_[node];
              manip_->graph()->getConfigProjectorStats
                (ni.id, ni.configStat.out(), ni.pathStat.out());
              float sr = (ni.configStat->nbObs > 0)
                ? (float)ni.configStat->success/(float)ni.configStat->nbObs
                : 0.f / 0.f;
              QString colorcode = (ni.configStat->nbObs > 0)
                ? QColor (255,(int)(sr*255),(int)(sr*255)).name()
                : "white";
              const QString& fillcolor = node->getAttribute("fillcolor");
              if (!(fillcolor == colorcode)) {
                  node->setAttribute("fillcolor", colorcode);
                  node->updateLayout();
                }
              continue;
            }
          QGVEdge* edge = dynamic_cast <QGVEdge*> (elmt);
          if (edge) {
              EdgeInfo& ei = edgeInfos_[edge];
              manip_->graph()->getConfigProjectorStats
                (ei.id, ei.configStat.out(), ei.pathStat.out());
              manip_->graph()->getEdgeStat
                (ei.id, ei.errors.out(), ei.freqs.out());
              float sr = (ei.configStat->nbObs > 0)
                ? (float)ei.configStat->success/(float)ei.configStat->nbObs
                : 0.f / 0.f;
              QString colorcode = (ei.configStat->nbObs > 0)
                ? QColor (255 - (int)(sr*255),0,0).name()
                : "";
              const QString& color = edge->getAttribute("color");
              if (!(color == colorcode)) {
                  edge->setAttribute("color", colorcode);
                  edge->updateLayout();
                }
              continue;
            }
        }
      scene_->update();
      selectionChanged ();
    }

    void HppManipulationGraphWidget::nodeContextMenu(QGVNode *node)
    {
      const NodeInfo& ni = nodeInfos_[node];
      hpp::ID id = currentId_;
      currentId_ = ni.id;

      QMenu cm ("Node context menu", this);
      foreach (GraphAction* action, nodeContextMenuActions_) {
          cm.addAction (action);
      }
      cm.exec(QCursor::pos());

      currentId_ = id;
    }

    void HppManipulationGraphWidget::nodeDoubleClick(QGVNode *node)
    {
      QMessageBox::information(this, tr("Node double clicked"), tr("Node %1").arg(node->label()));
    }

    void HppManipulationGraphWidget::edgeContextMenu(QGVEdge *edge)
    {
      const EdgeInfo& ei = edgeInfos_[edge];
      hpp::ID id = currentId_;
      currentId_ = ei.id;

      QMenu cm ("Edge context menu", this);
      foreach (GraphAction* action, edgeContextMenuActions_) {
          cm.addAction (action);
      }
      cm.exec(QCursor::pos());

      currentId_ = id;
    }

    void HppManipulationGraphWidget::edgeDoubleClick(QGVEdge *edge)
    {
      EdgeInfo& ei = edgeInfos_[edge];
      bool ok;
      ::CORBA::Long w = QInputDialog::getInt(
            this, "Update edge weight",
            tr("Edge %1 weight").arg(edge->label()),
            ei.weight, 0, std::numeric_limits<int>::max(), 1,
            &ok);
      if (ok) {
          updateWeight(ei, w);
          edge->updateLayout();
        }
    }

    void HppManipulationGraphWidget::selectionChanged()
    {
      QList <QGraphicsItem*> items = scene_->selectedItems();
      currentId_ = -1;
      if (items.size() == 0) {
          elmtInfo_->setText ("No info");
        }
      QString weight;
      if (items.size() == 1) {
          QString type, name; int success, error, nbObs; ::hpp::ID id;
          QGVNode* node = dynamic_cast <QGVNode*> (items.first());
          QGVEdge* edge = dynamic_cast <QGVEdge*> (items.first());
          QString end;
          if (node) {
              type = "Node";
              name = node->label();
              const NodeInfo& ni = nodeInfos_[node];
              id = ni.id;
              currentId_ = id;
              success = ni.configStat->success;
              error = ni.configStat->error;
              nbObs = ni.configStat->nbObs;
            } else if (edge) {
              type = "Edge";
              const EdgeInfo& ei = edgeInfos_[edge];
              name = ei.name;
              id = ei.id;
              currentId_ = id;
              weight = QString ("<li>Weight: %1</li>").arg(ei.weight);
              success = ei.configStat->success;
              error = ei.configStat->error;
              nbObs = ei.configStat->nbObs;
              end = "<p>Extension results<ul>";
              for (std::size_t i = 0; i < std::min(ei.errors->length(),ei.freqs->length()); ++i) {
                end.append (
                    QString ("<li>%1: %2</li>")
                    .arg(QString(ei.errors.in()[i]))
                    .arg(        ei.freqs.in()[i]  ));
              }
              end.append("</ul></p>");
            } else {
              return;
            }
          elmtInfo_->setText (
            QString ("<h4>%1 %2</h4><ul>"
              "<li>Id: %3</li>"
              "%4"
              "<li>Success: %5</li>"
              "<li>Error: %6</li>"
              "<li>Nb observations: %7</li>"
              "</ul>%8")
            .arg (type).arg (Qt::escape (name)).arg(id).arg (weight)
            .arg(success).arg(error).arg(nbObs).arg(end));
        }
    }

    void HppManipulationGraphWidget::startStopUpdateStats(bool start)
    {
      if (start) updateStatsTimer_->start ();
      else       updateStatsTimer_->stop ();
    }

    HppManipulationGraphWidget::NodeInfo::NodeInfo () : id (-1)
    {
      initConfigProjStat (configStat.out());
      initConfigProjStat (pathStat.out());
    }

    HppManipulationGraphWidget::EdgeInfo::EdgeInfo () : id (-1), edge (NULL)
    {
      initConfigProjStat (configStat.out());
      initConfigProjStat (pathStat.out());
      errors = new Names_t();
      freqs = new intSeq();
    }

    void HppManipulationGraphWidget::updateWeight (EdgeInfo& ei, bool get)
    {
      if (get) ei.weight = manip_->graph()->getWeight(ei.id);
      if (ei.edge == NULL) return;
      if (ei.weight <= 0) {
          ei.edge->setAttribute("style", "dashed");
          ei.edge->setAttribute("penwidth", "1");
        } else {
          ei.edge->setAttribute("style", "filled");
          ei.edge->setAttribute("penwidth", QString::number(1 + (ei.weight - 1  / 5)));
        }
    }

    void HppManipulationGraphWidget::updateWeight (EdgeInfo& ei, const ::CORBA::Long w)
    {
      manip_->graph()->setWeight(ei.id, w);
      ei.weight = w;
      updateWeight (ei, false);
    }
  }
}
