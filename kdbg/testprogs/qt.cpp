#include <qmap.h>
#include <qvaluelist.h>
#include <qvaluevector.h>
#include <qstring.h>
#include <qrect.h>
#include <iostream>

template<typename T>
void test_sharing(const T& input)
{
	// a copy should increase the share counter
	T copy = input;

	// a const interator should not detach the copy
	typename T::const_iterator cit = copy.constBegin();
	std::cout << *cit << std::endl;

	// a non-const iterator should detach the copy
	typename T::iterator it = copy.begin();
	std::cout << *it << std::endl;
}

int main()
{
	QMap<QString,int> str2int;
	str2int["foo"] = 42;
	test_sharing(str2int);

	QValueList<int> ints;
	ints.push_back(42);
	test_sharing(ints);

	QValueVector<double> vals(6, 47.11);
	vals.push_back(42);
	test_sharing(vals);

	QRect r(10,20, 130, 240);
	QPoint p = r.topLeft();
	QPoint q = r.bottomRight();
	std::cout << r.width() << r.height() << p.x() << q.y() << std::endl;
}
