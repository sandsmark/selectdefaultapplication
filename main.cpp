#include "selectdefaultapplication.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	SelectDefaultApplication w;
	w.show();

	return a.exec();
}
