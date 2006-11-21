#include <wchar.h>
#include <stdio.h>
#include <string>

struct WChar {
	const wchar_t* cwstr;
	wchar_t* wstr;
};

int main()
{
   int j=3;
   const wchar_t* nullPtr = 0;
   const wchar_t* uninitializedPtr = (const wchar_t*)0xdeadbeef;
   const wchar_t* str = L"abc";
   wchar_t* str2 = L"def";
   const char* shortStr = "12345";

   WChar s = { 0, L"Some string" };
   s.cwstr = s.wstr;

   wchar_t wstr[64], *wstrPtr = wstr;

   wprintf(L"wide string: %S\n", str);

   for (int i=0; i<j; ++i)
   {
      swprintf(wstr, 63, L"%d. wide string: %S\n", i+1, str);
      wprintf(L"%S\n", wstr);
   }
}
