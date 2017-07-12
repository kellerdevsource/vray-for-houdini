//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <QtNetwork/QTcpSocket>

int main(int, char const*[])
{
	QTcpSocket socket;
	if (!socket.bind(424242)) {
		return 1;
	}

	while (true) {}

	return 0;
}
