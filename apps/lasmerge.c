/***************************************************************************
 * $Id: lasmerge.cpp 87 2007-12-16 02:40:46Z hobu $
 * $Date: 2007-12-15 20:40:46 -0600 (Sat, 15 Dec 2007) $
 *
 * Project: libLAS -- C/C++ read/write library for LAS LIDAR data
 * Purpose: LAS file merging
 * Author:  Martin Isenburg isenburg@cs.unc.edu 
 ***************************************************************************
 * Copyright (c) 2007, Martin Isenburg isenburg@cs.unc.edu 
 *
 * See LICENSE.txt in this source distribution for more information.
 **************************************************************************/


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "liblas.h"


void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"lasmerge -i lasfiles.txt -o out.las\n");
  fprintf(stderr,"lasmerge -i file1.las -i file2.las -i file3.las -o out.las\n");
  fprintf(stderr,"lasmerge -i file1.las -i file2.las -olas > out.las\n");
  fprintf(stderr,"lasmerge -i lasfiles.txt -scale 0.01 -verbose -o out.las\n");
  fprintf(stderr,"lasmerge -h\n");
  exit(1);
}

void ptime(const char *const msg)
{
  float t= ((float)clock())/CLOCKS_PER_SEC;
  fprintf(stderr, "cumulative CPU time thru %s = %f\n", msg, t);
}

int main(int argc, char *argv[])
{
  int i;
  int verbose = FALSE;
  int num_file_name_in = 0;
  int alloced_file_name_in = 32;
  char** file_names_in = (char**)malloc(sizeof(char*)*alloced_file_name_in);
  char* file_name_out = 0;
  int olas = FALSE;
  int olaz = FALSE;
  double xyz_scale[3] = {0.0, 0.0, 0.0};
  double xyz_offset[3] = {0.0, 0.0, 0.0};

  double minx, miny, maxx, maxy, minz, maxz;
  double x_scale_factor, y_scale_factor, z_scale_factor;

  
  LASHeaderH merged_header=NULL;
  LASHeaderH header = NULL;
  LASReaderH reader = NULL;
  LASWriterH writer = NULL;
  LASPointH p = NULL;
  
  FILE* file = NULL;

  int smallest_int = (1<<31)+10;
  int largest_int = smallest_int-1-20;

  int same = TRUE;
    
  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-verbose") == 0)
    {
      verbose = TRUE;
    }
    else if (strcmp(argv[i],"-h") == 0)
    {
      usage();
    }
    else if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      if (num_file_name_in == alloced_file_name_in)
      {
        alloced_file_name_in *= 2;
        file_names_in = (char**)realloc(file_names_in,sizeof(char*)*alloced_file_name_in);
      }
      file_names_in[num_file_name_in] = argv[i];
      num_file_name_in++;
    }
    else if (strcmp(argv[i],"-o") == 0)
    {
      i++;
      file_name_out = argv[i];
    }
    else if (strcmp(argv[i],"-scale") == 0)
    {

			i++;
			sscanf(argv[i], "%lf", &(xyz_scale[2]));
 			xyz_scale[0] = xyz_scale[1] = xyz_scale[2];
    }
    else if (strcmp(argv[i],"-xyz_scale") == 0)
    {

			i++;
			sscanf(argv[i], "%lf", &(xyz_scale[0]));
			i++;
			sscanf(argv[i], "%lf", &(xyz_scale[1]));
			i++;
			sscanf(argv[i], "%lf", &(xyz_scale[2]));
    }
    else if (strcmp(argv[i],"-xyz_offset") == 0)
    {

			i++;
			sscanf(argv[i], "%lf", &(xyz_offset[0]));
			i++;
			sscanf(argv[i], "%lf", &(xyz_offset[1]));
			i++;
			sscanf(argv[i], "%lf", &(xyz_offset[2]));
    }
    else if (strcmp(argv[i],"-olas") == 0)
    {
      olas = TRUE;
    }
    else if (strcmp(argv[i],"-olaz") == 0)
    {
      olaz = TRUE;
    }
    else if (i == argc - 2 && num_file_name_in == 0 && file_name_out == 0)
    {
      file_names_in[0] = argv[i];
      num_file_name_in = 1;
    }
    else if (i == argc - 1 && num_file_name_in == 0 && file_name_out == 0)
    {
      file_names_in[0] = argv[i];
      num_file_name_in = 1;
    }
    else if (i == argc - 1 && num_file_name_in != 0 && file_name_out == 0)
    {
      file_name_out = argv[i];
    }
  }

  if (num_file_name_in == 0)
  {
    fprintf(stderr, "ERROR: no input specified\n");
    usage();
  }

  if (num_file_name_in == 1 && strstr(file_names_in[0],".las") == 0 && strstr(file_names_in[0],".LAS") == 0)
  {
    char line[512];

    num_file_name_in = 0;
    file = fopen(file_names_in[0], "r");
	  while (fgets(line, sizeof(char) * 512, file))
    {
      if (strstr(line,".las") || strstr(line,".LAS") )
      {
        if (num_file_name_in == alloced_file_name_in)
        {
          alloced_file_name_in *= 2;
          file_names_in = (char**)realloc(file_names_in,sizeof(char*)*alloced_file_name_in);
        }
        file_names_in[num_file_name_in] = strdup(line);
        i = strlen(file_names_in[num_file_name_in]) - 1;
        while (i && file_names_in[num_file_name_in][i] != 's' && file_names_in[num_file_name_in][i] != 'S' && file_names_in[num_file_name_in][i] != 'z' && file_names_in[num_file_name_in][i] != 'Z') i--;
        if (i)
        {
          file_names_in[num_file_name_in][i+1] = '\0';
          num_file_name_in++;
        }
        else
        {
          fprintf(stderr, "WARNING: cannot parse line '%s'\n",line);
        }
      }
      else
      {
        fprintf(stderr, "WARNING: no a valid LAS file name '%s'\n",line);
      }
    }
  }


  if (verbose) ptime("starting first pass.");
  fprintf(stderr, "first pass ... reading headers of %d LAS files...\n", num_file_name_in);

  for (i = 0; i < num_file_name_in; i++)
  {
      reader = LASReader_Create(file_names_in[i]);
      if (!reader) { 
          fprintf(stdout, 
                  "Error! %d, %s, in method %s\n",
                  LASError_GetLastErrorNum(),
                  LASError_GetLastErrorMsg(),
                  LASError_GetLastErrorMethod()
                 ); 
          exit(-1);
      } 
      
      header = LASReader_GetHeader(reader);
      if (!header) { 
          fprintf(stdout, 
                  "Error! %d, %s, in method %s\n",
                  LASError_GetLastErrorNum(),
                  LASError_GetLastErrorMsg(),
                  LASError_GetLastErrorMethod()
                 ); 
          exit(-1);
      } 


    if (i == 0)
    {
      merged_header = LASReader_GetHeader(reader);
    }
    else
    {
        LASHeader_SetPointRecordsCount(merged_header, LASHeader_GetPointRecordsCount(merged_header)+LASHeader_GetPointRecordsCount(header));
        LASHeader_SetPointRecordsByReturnCount(merged_header, 0, LASHeader_GetPointRecordsByReturnCount(merged_header,0) + LASHeader_GetPointRecordsByReturnCount(header,0));
        LASHeader_SetPointRecordsByReturnCount(merged_header, 1, LASHeader_GetPointRecordsByReturnCount(merged_header,1) + LASHeader_GetPointRecordsByReturnCount(header,1));
        LASHeader_SetPointRecordsByReturnCount(merged_header, 2, LASHeader_GetPointRecordsByReturnCount(merged_header,2) + LASHeader_GetPointRecordsByReturnCount(header,2));
        LASHeader_SetPointRecordsByReturnCount(merged_header, 3, LASHeader_GetPointRecordsByReturnCount(merged_header,3) + LASHeader_GetPointRecordsByReturnCount(header,3));
        LASHeader_SetPointRecordsByReturnCount(merged_header, 4, LASHeader_GetPointRecordsByReturnCount(merged_header,4) + LASHeader_GetPointRecordsByReturnCount(header,4));

        minx = LASHeader_GetMinX(merged_header);
        maxx = LASHeader_GetMaxX(merged_header);
        miny = LASHeader_GetMinY(merged_header);
        maxy = LASHeader_GetMaxY(merged_header);
        minz = LASHeader_GetMinZ(merged_header);
        maxz = LASHeader_GetMaxZ(merged_header);
        
        if (minx > LASHeader_GetMinX(header)) minx = LASHeader_GetMinX(header);
        if (maxx < LASHeader_GetMaxX(header)) maxx = LASHeader_GetMaxX(header);
        if (miny > LASHeader_GetMinY(header)) miny = LASHeader_GetMinY(header);
        if (maxy < LASHeader_GetMaxY(header)) maxy = LASHeader_GetMaxY(header);
        if (minz > LASHeader_GetMinZ(header)) minz = LASHeader_GetMinZ(header);
        if (maxz < LASHeader_GetMaxZ(header)) maxz = LASHeader_GetMaxZ(header);
        
        LASHeader_SetMin(merged_header, minx, miny, minz);
        LASHeader_SetMax(merged_header, maxx, maxy, maxz);
    }
    
    LASReader_Destroy(reader);
  }

  if (verbose)
  {
    fprintf(stderr, "  number_of_point_records %d\n", LASHeader_GetPointRecordsCount(merged_header));
    fprintf(stdout, "  number of points by return %d %d %d %d %d\n", LASHeader_GetPointRecordsByReturnCount(merged_header, 0), LASHeader_GetPointRecordsByReturnCount(merged_header, 1), LASHeader_GetPointRecordsByReturnCount(merged_header, 2), LASHeader_GetPointRecordsByReturnCount(merged_header, 3), LASHeader_GetPointRecordsByReturnCount(merged_header, 4));
    fprintf(stdout, "  min x y z                  %.6f %.6f %.6f\n", LASHeader_GetMinX(merged_header), LASHeader_GetMinY(merged_header), LASHeader_GetMinZ(merged_header));
    fprintf(stdout, "  max x y z                  %.6f %.6f %.6f\n", LASHeader_GetMaxX(merged_header), LASHeader_GetMaxY(merged_header), LASHeader_GetMaxZ(merged_header));
  }

  if (xyz_scale)
  {
      LASHeader_SetScale(merged_header, xyz_scale[0], xyz_scale[1], xyz_scale[2]);
  }

  if (xyz_offset)
  {
      LASHeader_SetOffset(merged_header, xyz_offset[0], xyz_offset[1], xyz_offset[2] );
  }

  x_scale_factor = LASHeader_GetScaleX(merged_header);
  if (((LASHeader_GetMaxX(merged_header) - LASHeader_GetOffsetX(merged_header)) / LASHeader_GetScaleX(merged_header)) > largest_int ||
      ((LASHeader_GetMinX(merged_header) - LASHeader_GetOffsetX(merged_header)) / LASHeader_GetScaleX(merged_header)) < smallest_int )
  {
    x_scale_factor = 0.0000001;
    while (((LASHeader_GetMaxX(merged_header) - LASHeader_GetOffsetX(merged_header)) / x_scale_factor) > largest_int ||
          ((LASHeader_GetMinX(merged_header) - LASHeader_GetOffsetX(merged_header)) / x_scale_factor) < smallest_int )
    {
      x_scale_factor *= 10;
    }
    fprintf(stderr, "x_scale_factor of merged_header changed to %g\n", x_scale_factor);
  }

  y_scale_factor = LASHeader_GetScaleY(merged_header);
  if (((LASHeader_GetMaxY(merged_header) - LASHeader_GetOffsetY(merged_header)) / y_scale_factor) > largest_int ||
      ((LASHeader_GetMinY(merged_header) - LASHeader_GetOffsetY(merged_header)) / y_scale_factor) < smallest_int )
  {
    y_scale_factor = 0.0000001;
    while (((LASHeader_GetMaxY(merged_header) - LASHeader_GetOffsetY(merged_header)) / y_scale_factor) > largest_int ||
          ((LASHeader_GetMinY(merged_header) - LASHeader_GetOffsetY(merged_header)) / y_scale_factor) < smallest_int )
    {
      y_scale_factor *= 10;
    }
    fprintf(stderr, "y_scale_factor of merged_header changed to %g\n", y_scale_factor);
  }
  
  z_scale_factor = LASHeader_GetScaleZ(merged_header);
  if (((LASHeader_GetMaxZ(merged_header) - LASHeader_GetOffsetZ(merged_header)) / z_scale_factor) > largest_int ||
      ((LASHeader_GetMinZ(merged_header) - LASHeader_GetOffsetZ(merged_header)) / z_scale_factor) < smallest_int )
  {
    z_scale_factor = 0.0000001;

    while (((LASHeader_GetMaxZ(merged_header) - LASHeader_GetOffsetZ(merged_header)) / z_scale_factor) > largest_int ||
          ((LASHeader_GetMinZ(merged_header) - LASHeader_GetOffsetZ(merged_header)) / z_scale_factor) < smallest_int )

    {
      z_scale_factor *= 10;
    }
    fprintf(stderr, "z_scale_factor of merged_header changed to %g\n", z_scale_factor);
  }
  
  LASHeader_SetScale(merged_header, x_scale_factor, y_scale_factor, z_scale_factor);

  if (file_name_out == NULL && !olas && !olaz)
  {
    fprintf(stderr, "no output specified. exiting ...\n");
    exit(1);
  }

/*
  else if (olas || olaz)
  {
    file_out = stdout;
    if (olaz)
    {
      compression = 1;
    }
  }
*/

  writer = LASWriter_Create(file_name_out, merged_header);
  if (!writer) { 
      fprintf(stderr, 
              "Error! %d, %s, in method %s\n",
              LASError_GetLastErrorNum(),
              LASError_GetLastErrorMsg(),
              LASError_GetLastErrorMethod()
             ); 
      exit(-1);
  } 



  if (verbose) ptime("starting second pass.");
  fprintf(stderr, "second pass ... merge %d LAS files into one ...\n", num_file_name_in);

  for (i = 0; i < num_file_name_in; i++)
  {
      reader = LASReader_Create(file_names_in[i]);
      if (!reader) { 
          fprintf(stdout, 
                  "Error! %d, %s, in method %s\n",
                  LASError_GetLastErrorNum(),
                  LASError_GetLastErrorMsg(),
                  LASError_GetLastErrorMethod()
                 ); 
          exit(-1);
      } 
      header = LASReader_GetHeader(reader);
      if (!header) { 
          fprintf(stdout, 
                  "Error! %d, %s, in method %s\n",
                  LASError_GetLastErrorNum(),
                  LASError_GetLastErrorMsg(),
                  LASError_GetLastErrorMethod()
                 ); 
          exit(-1);
      } 

/*
    if (i == 0)
    {
      // copy variable header (and user data) from the first file
      for (unsigned int u = lasreader->header.header_size; u < lasreader->header.offset_to_point_data; u++)
      {
        fputc(fgetc(file_in),file_out);
      }
    }
*/


    same = TRUE;
    if (LASHeader_GetOffsetX(merged_header) != LASHeader_GetOffsetX(header)) same = FALSE;
    if (LASHeader_GetOffsetY(merged_header) != LASHeader_GetOffsetY(header)) same = FALSE;
    if (LASHeader_GetOffsetZ(merged_header) != LASHeader_GetOffsetZ(header)) same = FALSE;

    if (LASHeader_GetScaleX(merged_header) != LASHeader_GetScaleX(header)) same = FALSE;
    if (LASHeader_GetScaleY(merged_header) != LASHeader_GetScaleY(header)) same = FALSE;
    if (LASHeader_GetScaleZ(merged_header) != LASHeader_GetScaleZ(header)) same = FALSE;
    

    if (same)
    {
      p = LASReader_GetNextPoint(reader);
      while (p)
      {
          LASWriter_WritePoint(writer, p);
          p = LASReader_GetNextPoint(reader);
      }
    }
    else
    {
      p = LASReader_GetNextPoint(reader);
      while (p)
      {
          LASPoint_SetX(p,(0.5 + (LASPoint_GetX(p) - LASHeader_GetOffsetX(merged_header)) / LASHeader_GetScaleX(merged_header)));
          LASPoint_SetY(p,(0.5 + (LASPoint_GetY(p) - LASHeader_GetOffsetY(merged_header)) / LASHeader_GetScaleY(merged_header)));
          LASPoint_SetZ(p,(0.5 + (LASPoint_GetZ(p) - LASHeader_GetOffsetZ(merged_header)) / LASHeader_GetScaleZ(merged_header)));
          LASWriter_WritePoint(writer, p);
          p = LASReader_GetNextPoint(reader);
      }
    }

    LASReader_Destroy(reader);
  }

  LASWriter_Destroy(writer);

  if (verbose) ptime("done.");

  return 0;
}