#include <string>
#include <sstream>
#include <vector>
#include "helpers.h"
#include <vdr/skins.h>

cOsd* CreateOsd(int Left, int Top, int Width, int Height) 
{
   cOsd* osd = cOsdProvider::NewOsd(Left, Top);

   if (osd) 
   {
      tArea Area = { 0, 0, Width, Height,  32 };
      if (osd->SetAreas(&Area, 1) == oeOk) 
      {
         return osd;
      }
   }
    return NULL;
}

void DrawBlendedBackground(cPixmap *pixmap, int xStart, int width, tColor color, tColor colorBlending, bool fromTop)
{
   int height = pixmap->ViewPort().Height();
   int numSteps = 16;
   int alphaStep = 0x0F;
   int alpha = 0x00;
   int step, begin, end;

   if (fromTop) 
   {
      step = 1;
      begin = 0;
      end = numSteps;
   } 
   else 
   {
      step = -1;
      begin = height;
      end = height - numSteps;
   }

   tColor clr;
   bool cont = true;
   
   for (int i = begin; cont; i = i + step) 
   {
      clr = AlphaBlend(color, colorBlending, alpha);
      pixmap->DrawRectangle(cRect(xStart,i,width,1), clr);
      alpha += alphaStep;
      
      if (i == end)
         cont = false;
   }
}

void DrawRoundedCorners(cPixmap *p, int radius, int x, int y, int width, int height) {
    if (radius > 2) {
        p->DrawEllipse(cRect(x, y, radius, radius), clrTransparent, -2);
        p->DrawEllipse(cRect(x + width - radius, y , radius, radius), clrTransparent, -1);
        p->DrawEllipse(cRect(x, y + height - radius, radius, radius), clrTransparent, -3);
        p->DrawEllipse(cRect(x + width - radius, y + height - radius, radius, radius), clrTransparent, -4);
    }
}

void DrawRoundedCornersWithBorder(cPixmap *p, tColor borderColor, int radius, int width, int height) {
    if (radius < 3)
        return;
    p->DrawEllipse(cRect(0,0,radius,radius), borderColor, -2);
    p->DrawEllipse(cRect(-1,-1,radius,radius), clrTransparent, -2);

    p->DrawEllipse(cRect(width-radius,0,radius,radius), borderColor, -1);
    p->DrawEllipse(cRect(width-radius+1,-1,radius,radius), clrTransparent, -1);

    p->DrawEllipse(cRect(0,height-radius,radius,radius), borderColor, -3);
    p->DrawEllipse(cRect(-1,height-radius+1,radius,radius), clrTransparent, -3);

    p->DrawEllipse(cRect(width-radius,height-radius,radius,radius), borderColor, -4);
    p->DrawEllipse(cRect(width-radius+1,height-radius+1,radius,radius), clrTransparent, -4);
}

cSize ScaleToFit(int widthMax, int heightMax, int widthOriginal, int heightOriginal) {
    int width = 1;
    int height = 1;

    if ((widthMax == 0)||(heightMax==0)||(widthOriginal==0)||(heightOriginal==0))
        return cSize(width, height);

    if ((widthOriginal <= widthMax) && (heightOriginal <= heightMax)) {
        width = widthOriginal;
        height = heightOriginal;
    } else if ((widthOriginal > widthMax) && (heightOriginal <= heightMax)) {
        width = widthMax;
        height = (double)width/(double)widthOriginal * heightOriginal;
    } else if ((widthOriginal <= widthMax) && (heightOriginal > heightMax)) {
        height = heightMax;
        width = (double)height/(double)heightOriginal * widthOriginal;
    } else {
        width = widthMax;
        height = (double)width/(double)widthOriginal * heightOriginal;
        if (height > heightMax) {
            height = heightMax;
            width = (double)height/(double)heightOriginal * widthOriginal;
        }
    }
    return cSize(width, height);
}

int Minimum(int a, int b, int c, int d, int e, int f) {
    int min = a;
    if (b < min) min = b;
    if (c < min) min = c;
    if (d < min) min = d;
    if (e < min) min = e;
    if (f < min) min = f;
    return min;
}

std::string CutText(std::string text, int width, const cFont *font) {
    if (width <= font->Size())
        return text.c_str();
    cTextWrapper twText;
    twText.Set(text.c_str(), font, width);
    std::string cuttedTextNative = twText.GetLine(0);
    std::stringstream sstrText;
    sstrText << cuttedTextNative << "...";
    std::string cuttedText = sstrText.str();
    int actWidth = font->Width(cuttedText.c_str());
    if (actWidth > width) {
        int overlap = actWidth - width;
        int charWidth = font->Width(".");
        if (charWidth == 0)
            charWidth = 1;
        int cutChars = overlap / charWidth;
        if (cutChars > 0) {
            cuttedTextNative = cuttedTextNative.substr(0, cuttedTextNative.length() - cutChars);
            std::stringstream sstrText2;
            sstrText2 << cuttedTextNative << "...";
            cuttedText = sstrText2.str();
        }
    }
    return cuttedText;
}

std::string StrToLowerCase(std::string str) {
    std::string lowerCase = str;
    const int length = lowerCase.length();
    for(int i=0; i < length; ++i) {
        lowerCase[i] = std::tolower(lowerCase[i]);
    }
    return lowerCase;
}

// split: receives a char delimiter; returns a vector of strings
// By default ignores repeated delimiters, unless argument rep == 1.
std::vector<std::string>& splitstring::split(char delim, int rep) {
    if (!flds.empty()) flds.clear();  // empty vector if necessary
    std::string work = data();
    std::string buf = "";
    unsigned int i = 0;
    while (i < work.length()) {
        if (work[i] != delim)
            buf += work[i];
        else if (rep == 1) {
            flds.push_back(buf);
            buf = "";
        } else if (buf.length() > 0) {
            flds.push_back(buf);
            buf = "";
        }
        i++;
    }
    if (!buf.empty())
        flds.push_back(buf);
    return flds;
}
