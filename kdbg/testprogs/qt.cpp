#include <QtCore/QString>
#include <QtCore/QRect>
#include <iostream>

int main()
{
	QString str;
	str = QLatin1String("A test string");

	QRect r(10,20, 130, 240);
	QPoint p = r.topLeft();
	QPoint q = r.bottomRight();
	str = QLatin1String("New text");
	std::cout << r.width() << r.height() << p.x() << q.y() << std::endl;
}
