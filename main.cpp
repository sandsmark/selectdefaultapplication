#include "selectdefaultapplication.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

	QApplication a(argc, argv);
	a.setApplicationVersion("2.0");
	a.setApplicationDisplayName("Select Default Application");

	QCommandLineParser parser;
	parser.setApplicationDescription(
		"A very simple application that lets you define default applications on Linux in a sane way.");
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption verbose(QStringList() << "V"
						 << "verbose",
				   "Print verbose information about how the desktop files are parsed");

	parser.addOption(verbose);
	parser.process(a);

	SelectDefaultApplication w(nullptr, parser.isSet(verbose));
	w.show();

	return a.exec();
}
