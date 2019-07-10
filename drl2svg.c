// Excellon DRL to SVG convertor
// Copyright © 2019 Adrian Kennard Andrews & Arnold Ltd
// Released under GPL, see LICENCE file

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <time.h>
#include <err.h>
#include <axl.h>

int debug = 0;

int
main (int argc, const char *argv[])
{
   const char *outfile = NULL;
   int scale=100000;
   double width=0,height=0;

   poptContext optCon;          // context for parsing command-line options
   const struct poptOption optionsTable[] = {
      {"outfile", 'o', POPT_ARG_STRING, &outfile, 0, "Outfile", "filename"},
      {"width", 'w', POPT_ARG_DOUBLE, &width, 0, "Width", "mm"},
      {"height", 'h', POPT_ARG_DOUBLE, &height, 0, "Height", "mm"},
      {"scale", 's', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT, &scale, 0, "Scale", "N"},
      {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
      POPT_AUTOHELP {}
   };

   optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
   poptSetOtherOptionHelp (optCon, "[drl files]");

   int c;
   if ((c = poptGetNextOpt (optCon)) < -1)
      errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

   if (!poptPeekArg (optCon))
   {
      poptPrintUsage (optCon, stderr, 0);
      return -1;
   }

   long long maxx = 0,
      maxy = 0;

   xml_t svg = xml_tree_new ("svg");
   xml_element_set_namespace (svg, xml_namespace (svg, NULL, "http://www.w3.org/2000/svg"));
   xml_add (svg, "@version", "1.1");

   const char *fn;
   while ((fn = poptGetArg (optCon)))
   {                            // Add files
      FILE *f = fopen (fn, "r");
      if (!f)
         err (1, "Cannot open %s", fn);
      xml_t g = xml_element_add (svg, "g");
      xml_add(g,"@stroke-linecap","round");
      xml_add(g,"@stroke-linejoin","round");
      xml_addf (g, "@id", fn);
      if(width&&height)
      {
	      xml_t r=xml_element_add(g,"rect");
	      xml_addf(r,"@width","%lld",(long long)width*scale);
	      xml_addf(r,"@height","%lld",(long long)height*scale);
	      xml_addf(r,"@fill","none");
	      xml_addf(r,"@stroke","black");
      }
      char line[1000];
      long long toolsize[100] = { };
      unsigned int tool = 0;
      long long x = 0,
         y = 0,
         lx = 0,
         ly = 0;
      char *path = NULL;
      size_t pathlen = 0;
      FILE *pathfile = NULL;
      int pathn = 0;
      void addpath (void)
      {
         if (x == lx && y == ly)
            return;
         if (x+toolsize[tool]/2 > maxx)
            maxx = x+toolsize[tool]/2;
         if (y+toolsize[tool]/2 > maxy)
            maxy = y+toolsize[tool]/2;
         lx = x;
         ly = y;
         if (!pathfile)
         {
            pathn = 0;
            pathfile = open_memstream (&path, &pathlen);
            fprintf (pathfile, "M");
         } else
            fprintf (pathfile, "L");
         fprintf (pathfile, "%llu %llu", lx, ly);
         pathn++;
      }
      void endofpath (void)
      {
         if (!pathfile)
            return;
         fclose (pathfile);
         if (pathn == 1)
         {
            xml_t c = xml_element_add (g, "circle");
            xml_addf (c, "@cx", "%llu", lx);
            xml_addf (c, "@cy", "%llu", ly);
            xml_addf (c, "@r", "%llu", toolsize[tool] / 2);
            xml_addf (c, "@fill", "black");
         } else
         {
            xml_t p = xml_element_add (g, "path");
            xml_addf (p, "@stroke-width", "%lld", toolsize[tool]);
            xml_addf (p, "@stroke", "black");
            xml_addf (p, "@d", path);
         }
         free (path);
         pathfile = NULL;
      }
      while (fgets (line, sizeof (line), f) > 0)
      {
         if (!strncmp (line, "METRIC", 6))
            continue;           // Expected
         char *p = line;
         long double c = 0;
         int g = -1,
            m = 0,
            t = 0;
         while (*p && isalpha (*p))
         {
            char tag = *p++;
            if (tag == 'X')
               x = strtoll (p, &p, 10);
            else if (tag == 'Y')
               y = strtoll (p, &p, 10);
            else if (tag == 'C')
               c = strtold (p, &p);
            else if (tag == 'M')
               m = strtoimax (p, &p, 10);
            else if (tag == 'G')
               g = strtoimax (p, &p, 10);
            else if (tag == 'T')
               t = strtoimax (p, &p, 10);
            else
               warnx ("Unknown tag %c", tag);
         }
         if (t)
         {                      // Tool change or setting
            endofpath ();
            if (t >= sizeof (toolsize) / sizeof (*toolsize))
               errx (1, "Bad tool %u", t);
            tool = t;
            if (c)
               toolsize[tool] = c * scale;       // Set tool size
            else if (!toolsize[tool])
               errx (1, "No size for tool %u", tool);
         }
         if (!m && g <= 0)
            endofpath ();
         addpath ();
      }
      endofpath ();
      fclose (f);
   }
   poptFreeContext (optCon);

   if(width)maxx=scale*width;
   if(height)maxy=scale*height;

   xml_addf (svg, "@width", "%.3llfmm", (long double) maxx / scale);
   xml_addf (svg, "@height", "%.3llfmm", (long double) maxy / scale);
   xml_addf(svg,"@viewBox","0 0 %lld %lld",maxx,maxy);
   // Output
   FILE *o = stdout;
   if (outfile)
      o = fopen (outfile, "w");
   if (!o)
      err (1, "Cannot open %s", outfile);
   xml_write (o, svg);
   fclose (o);

   return 0;
}
