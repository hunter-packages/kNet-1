/* Copyright 2010 Jukka Jyl�nki

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

/** @file NetworkDialog.cpp
	@brief */

#include <QUiLoader>
#include <QFile>
#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>

#include "kNet/qt/NetworkDialog.h"
#include "kNet/qt/MessageConnectionDialog.h"
#include "kNet/DebugMemoryLeakCheck.h"

namespace kNet
{

const int dialogUpdateInterval = 1000;

NetworkDialog::NetworkDialog(QWidget *parent, Network *network_)
:network(network_), QWidget(parent)
{
	QUiLoader loader;
	QFile file("NetworkDialog.ui");
	file.open(QFile::ReadOnly);
	QWidget *myWidget = loader.load(&file, this);
	file.close();
/*
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(myWidget);
	setLayout(layout);
*/
	QTreeWidget *connectionsTree = findChild<QTreeWidget*>("connectionsTree");
	if (connectionsTree)
		connect(connectionsTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(ItemDoubleClicked(QTreeWidgetItem *)));

	updateTimer = new QTimer(this);
	connect(updateTimer, SIGNAL(timeout()), this, SLOT(Update()));
	Update();
}

class MessageConnectionTreeItem : public QTreeWidgetItem
{
public:
	MessageConnectionTreeItem(QTreeWidgetItem *parent, Ptr(MessageConnection) connection_)
	:QTreeWidgetItem(parent), connection(connection_)
	{
		if (connection)
			setText(0, connection->ToString().c_str());
	}

	Ptr(MessageConnection) connection;
};

QTreeWidgetItem *NewTreeItemFromString(QTreeWidget *parent, const char *str)
{
	QTreeWidgetItem *item = new QTreeWidgetItem(parent);
	item->setText(0, str);
	return item;
}

void NetworkDialog::Update()
{
	if (!network)
		return;

	QLabel *machineIp = findChild<QLabel*>("machineIP");
	if (!machineIp)
		return;
	QLabel *numRunningThreads = findChild<QLabel*>("numRunningThreads");
	if (!numRunningThreads)
		return;
	QTreeWidget *connectionsTree = findChild<QTreeWidget*>("connectionsTree");
	if (!connectionsTree)
		return;

	machineIp->setText(network->MachineIP());
	numRunningThreads->setText(QString::number(network->NumWorkerThreads()));

	connectionsTree->clear();

	Ptr(NetworkServer) server = network->GetServer();
	if (server)
	{
		QTreeWidgetItem *serverItem = NewTreeItemFromString(connectionsTree, server->ToString().c_str());
		connectionsTree->addTopLevelItem(serverItem);

		NetworkServer::ConnectionMap connections = server->GetConnections();
		for(NetworkServer::ConnectionMap::iterator iter = connections.begin(); iter != connections.end(); ++iter)
		{
			QTreeWidgetItem *connectionItem = new MessageConnectionTreeItem(serverItem, iter->second);
			serverItem->addChild(connectionItem);
			serverItem->setExpanded(true);
		}
	}

	updateTimer->start(dialogUpdateInterval);
}

void NetworkDialog::ItemDoubleClicked(QTreeWidgetItem *item)
{
	MessageConnectionTreeItem *msgItem = dynamic_cast<MessageConnectionTreeItem*>(item);
	if (!msgItem)
		return;

	MessageConnectionDialog *dialog = new MessageConnectionDialog(0, msgItem->connection);
	dialog->show();
}

} // ~kNet