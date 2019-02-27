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
   const wchar_t* str2 = L"def";
   const char* shortStr = "12345";
   std::string stdstr = "std::string";
   std::wstring stdwstr = L"std::wstring \x20ac";
   std::u16string stdu16str = u"std::u16string: \x20ac";
   std::u32string stdu32str = U"std::u32string: \x0001f604";

   wchar_t wstr[64] = { 0 },		// L'\0' <repeats...>
	*wstrPtr = wstr;
   wcscpy(wstr, L"Some string");	// L"str", '\0' <repeats...>
   wchar_t wc = wstr[0];

   WChar s = { 0, wstr };
   s.cwstr = s.wstr;

   wprintf(L"wide string: %S\n", str);

   for (int i=0; i<j; ++i)
   {
      swprintf(wstr, 63, L"%d. wide string: %S\n", i+1, str);
      wprintf(L"%S\n", wstr);
   }

   stdstr += std::string(30, stdstr.back());
   stdwstr += std::wstring(30, stdwstr.back());
   stdu16str += std::u16string(30, stdu16str.back());
   stdu32str += std::u32string(30, stdu32str.back());

   wprintf(L"done!\n");
}
