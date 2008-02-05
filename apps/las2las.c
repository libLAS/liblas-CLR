/***************************************************************************
 * $Id: las2las.cpp 87 2007-12-16 02:40:46Z hobu $
 * $Date: 2007-12-15 20:40:46 -0600 (Sat, 15 Dec 2007) $
 *
 * Project: libLAS -- C/C++ read/write library for LAS LIDAR data
 * Purpose: LAS translation with optional configuration
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
  fprintf(stderr,"las2las -remove_extra_header in.las out.las\n");
  fprintf(stderr,"las2las -i in.las -clip 63000000 483450000 63050000 483500000 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -eliminate_return 2 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -eliminate_scan_angle_above 15 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -eliminate_intensity_below 1000 -olas > out.las\n");
  fprintf(stderr,"las2las -i in.las -first_only -clip 63000000 483450000 63050000 483500000 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -last_only -eliminate_intensity_below 2000 -olas > out.las\n");
  fprintf(stderr,"las2las -h\n");
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
  char* file_name_in = 0;
  char* file_name_out = 0;
  int olas = FALSE;
  int olaz = FALSE;
  int clip_xy_min[2];
  int clip_xy_max[2];
  int clip = FALSE;
  int remove_extra_header = FALSE;
  int elim_return = 0;
  int elim_scan_angle_above = 0;
  int elim_intensity_below = 0;
  int first_only = FALSE;
  int last_only = FALSE;

  LASReaderH reader = NULL;
  LASHeaderH header = NULL;
  LASHeaderH surviving_header = NULL;
  LASPointH p = NULL;
  LASWriterH writer = NULL;
  
  int first_surviving_point = TRUE;
  unsigned int surviving_number_of_point_records = 0;
  unsigned int surviving_number_of_points_by_return[] = {0,0,0,0,0,0,0,0};
  LASPointH surviving_point_min = NULL;
  LASPointH surviving_point_max = NULL;
  double surviving_gps_time_min;
  double surviving_gps_time_max;

  int clipped = 0;
  int eliminated_return = 0;
  int eliminated_scan_angle = 0;
  int eliminated_intensity = 0;
  int eliminated_first_only = 0;
  int eliminated_last_only = 0;

  double minx, maxx, miny, maxy, minz, maxz;
  
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
      file_name_in = argv[i];
    }
    else if (strcmp(argv[i],"-o") == 0)
    {
      i++;
      file_name_out = argv[i];
    }
    else if (strcmp(argv[i],"-olas") == 0)
    {
      olas = TRUE;
    }
    else if (strcmp(argv[i],"-olaz") == 0)
    {
      olaz = TRUE;
    }
    else if (strcmp(argv[i],"-clip") == 0 || strcmp(argv[i],"-clip_xy") == 0)
    {
      i++;
      clip_xy_min[0] = atoi(argv[i]);
      i++;
      clip_xy_min[1] = atoi(argv[i]);
      i++;
      clip_xy_max[0] = atoi(argv[i]);
      i++;
      clip_xy_max[1] = atoi(argv[i]);
      clip = TRUE;
    }
    else if (strcmp(argv[i],"-eliminate_return") == 0 || strcmp(argv[i],"-elim_return") == 0 || strcmp(argv[i],"-elim_ret") == 0)
    {
      i++;
      elim_return |= (1 << atoi(argv[i]));
    }
    else if (strcmp(argv[i],"-eliminate_scan_angle_above") == 0 || strcmp(argv[i],"-elim_scan_angle_above") == 0)
    {
      i++;
      elim_scan_angle_above = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-eliminate_intensity_below") == 0 || strcmp(argv[i],"-elim_intensity_below") == 0)
    {
      i++;
      elim_intensity_below = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-first_only") == 0)
    {
      first_only = TRUE;
    }
    else if (strcmp(argv[i],"-last_only") == 0)
    {
      last_only = TRUE;
    }
    else if (strcmp(argv[i],"-remove_extra_header") == 0)
    {
      remove_extra_header = TRUE;
    }
    else if (i == argc - 2 && file_name_in == 0 && file_name_out == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in == 0 && file_name_out == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in != 0 && file_name_out == 0)
    {
      file_name_out = argv[i];
    }
  }

  
  if (file_name_in)
  {
      reader = LASReader_Create(file_name_in);
      if (!reader) { 
          fprintf(stderr, 
                  "Error! %d, %s, in method %s\n",
                  LASError_GetLastErrorNum(),
                  LASError_GetLastErrorMsg(),
                  LASError_GetLastErrorMethod()
                 ); 
          exit(-1);
      } 
  }
  else
  {
    fprintf(stderr, "ERROR: no input specified\n");
    usage();
  }


  header = LASReader_GetHeader(reader);
  if (!header) { 
      fprintf(stderr, 
              "Error! %d, %s, in method %s\n",
              LASError_GetLastErrorNum(),
              LASError_GetLastErrorMsg(),
              LASError_GetLastErrorMethod()
             ); 
      exit(-1);
  } 

  if (verbose) ptime("start.");
  fprintf(stderr, "first pass reading %d points ...\n", LASHeader_GetPointRecordsCount(header));

  p  = LASReader_GetNextPoint(reader);

  while (p)
  {

    if (last_only && LASPoint_GetReturnNumber(p) != LASPoint_GetNumberOfReturns(p))
    {
      eliminated_last_only++;
      p  = LASReader_GetNextPoint(reader);
      continue;
    }
    if (first_only && LASPoint_GetReturnNumber(p) != 1)
    {
      eliminated_first_only++;
      p  = LASReader_GetNextPoint(reader);
      continue;
    }
    
    if (clip && (LASPoint_GetX(p) < clip_xy_min[0] || LASPoint_GetY(p) < clip_xy_min[1]))
    {
      clipped++;
      p  = LASReader_GetNextPoint(reader);
      continue;
    }
    if (clip && (LASPoint_GetX(p) > clip_xy_max[0] || LASPoint_GetY(p) > clip_xy_max[1]))
    {
      clipped++;
      p  = LASReader_GetNextPoint(reader);
      continue;
    } 
    if (elim_return && (elim_return & (1 << LASPoint_GetReturnNumber(p))))
    {
      eliminated_return++;
      p  = LASReader_GetNextPoint(reader);
      continue;
    }
    if (elim_scan_angle_above && (LASPoint_GetScanAngleRank(p) > elim_scan_angle_above || LASPoint_GetScanAngleRank(p) < -elim_scan_angle_above))
    {
      eliminated_scan_angle++;
      p  = LASReader_GetNextPoint(reader);
      continue;
    }
    if (elim_intensity_below && LASPoint_GetIntensity(p) < elim_intensity_below)
    {
      eliminated_intensity++;
      p  = LASReader_GetNextPoint(reader);
      continue;
    }
    surviving_number_of_point_records++;
    surviving_number_of_points_by_return[LASPoint_GetReturnNumber(p)-1]++;
    if (first_surviving_point)
    {
      surviving_point_min = LASPoint_Copy(p);
      surviving_point_max = LASPoint_Copy(p);
      surviving_gps_time_min = LASPoint_GetTime(p);
      surviving_gps_time_max = LASPoint_GetTime(p);
      first_surviving_point = FALSE;
    }
    else
    {
      if (LASPoint_GetX(p) < LASPoint_GetX(surviving_point_min)) LASPoint_SetX(surviving_point_min,LASPoint_GetX(p));
      else if (LASPoint_GetX(p) > LASPoint_GetX(surviving_point_max)) LASPoint_SetX(surviving_point_max,LASPoint_GetX(p));

      if (LASPoint_GetY(p) < LASPoint_GetY(surviving_point_min)) LASPoint_SetY(surviving_point_min,LASPoint_GetY(p));
      else if (LASPoint_GetY(p) > LASPoint_GetY(surviving_point_max)) LASPoint_SetY(surviving_point_max,LASPoint_GetY(p));

      if (LASPoint_GetZ(p) < LASPoint_GetZ(surviving_point_min)) LASPoint_SetZ(surviving_point_min,LASPoint_GetZ(p));
      else if (LASPoint_GetZ(p) > LASPoint_GetZ(surviving_point_max)) LASPoint_SetZ(surviving_point_max,LASPoint_GetZ(p));

      if (LASPoint_GetIntensity(p) < LASPoint_GetIntensity(surviving_point_min)) LASPoint_SetIntensity(surviving_point_min,LASPoint_GetIntensity(p));
      else if (LASPoint_GetIntensity(p) > LASPoint_GetIntensity(surviving_point_max)) LASPoint_SetIntensity(surviving_point_max,LASPoint_GetIntensity(p));

      if (LASPoint_GetFlightLineEdge(p) < LASPoint_GetFlightLineEdge(surviving_point_min)) LASPoint_SetFlightLineEdge(surviving_point_min,LASPoint_GetFlightLineEdge(p));
      else if (LASPoint_GetFlightLineEdge(p) > LASPoint_GetFlightLineEdge(surviving_point_max)) LASPoint_SetFlightLineEdge(surviving_point_max,LASPoint_GetFlightLineEdge(p));

      if (LASPoint_GetScanDirection(p) < LASPoint_GetScanDirection(surviving_point_min)) LASPoint_SetScanDirection(surviving_point_min,LASPoint_GetScanDirection(p));
      else if (LASPoint_GetScanDirection(p) > LASPoint_GetScanDirection(surviving_point_max)) LASPoint_SetScanDirection(surviving_point_max,LASPoint_GetScanDirection(p));

      if (LASPoint_GetNumberOfReturns(p) < LASPoint_GetNumberOfReturns(surviving_point_min)) LASPoint_SetNumberOfReturns(surviving_point_min,LASPoint_GetNumberOfReturns(p));
      else if (LASPoint_GetNumberOfReturns(p) > LASPoint_GetNumberOfReturns(surviving_point_max)) LASPoint_SetNumberOfReturns(surviving_point_max,LASPoint_GetNumberOfReturns(p));

      if (LASPoint_GetReturnNumber(p) < LASPoint_GetReturnNumber(surviving_point_min)) LASPoint_SetReturnNumber(surviving_point_min,LASPoint_GetReturnNumber(p));
      else if (LASPoint_GetReturnNumber(p) > LASPoint_GetReturnNumber(surviving_point_max)) LASPoint_SetReturnNumber(surviving_point_max,LASPoint_GetReturnNumber(p));

      if (LASPoint_GetClassification(p) < LASPoint_GetClassification(surviving_point_min)) LASPoint_SetClassification(surviving_point_min,LASPoint_GetClassification(p));
      else if (LASPoint_GetReturnNumber(p) > LASPoint_GetClassification(surviving_point_max)) LASPoint_SetClassification(surviving_point_max,LASPoint_GetClassification(p));

      if (LASPoint_GetScanAngleRank(p) < LASPoint_GetScanAngleRank(surviving_point_min)) LASPoint_SetScanAngleRank(surviving_point_min,LASPoint_GetScanAngleRank(p));
      else if (LASPoint_GetScanAngleRank(p) > LASPoint_GetScanAngleRank(surviving_point_max)) LASPoint_SetScanAngleRank(surviving_point_max,LASPoint_GetScanAngleRank(p));

      if (LASPoint_GetUserData(p) < LASPoint_GetUserData(surviving_point_min)) LASPoint_SetUserData(surviving_point_min,LASPoint_GetUserData(p));
      else if (LASPoint_GetUserData(p) > LASPoint_GetUserData(surviving_point_max)) LASPoint_SetUserData(surviving_point_max,LASPoint_GetUserData(p));

      if (LASPoint_GetTime(p) < LASPoint_GetTime(surviving_point_min)) LASPoint_SetTime(surviving_point_min,LASPoint_GetTime(p));
      else if (LASPoint_GetTime(p) > LASPoint_GetTime(surviving_point_max)) LASPoint_SetTime(surviving_point_max,LASPoint_GetTime(p));

/*
      if (lasreader->point.point_source_ID < surviving_point_min.point_source_ID) surviving_point_min.point_source_ID = lasreader->point.point_source_ID;
      else if (lasreader->point.point_source_ID > surviving_point_max.point_source_ID) surviving_point_max.point_source_ID = lasreader->point.point_source_ID;
*/
    }
  
    p  = LASReader_GetNextPoint(reader);
  }

  if (eliminated_first_only) fprintf(stderr, "eliminated based on first returns only: %d\n", eliminated_first_only);
  if (eliminated_last_only) fprintf(stderr, "eliminated based on last returns only: %d\n", eliminated_last_only);
  if (clipped) fprintf(stderr, "clipped: %d\n", clipped);
  if (eliminated_return) fprintf(stderr, "eliminated based on return number: %d\n", eliminated_return);
  if (eliminated_scan_angle) fprintf(stderr, "eliminated based on scan angle: %d\n", eliminated_scan_angle);
  if (eliminated_intensity) fprintf(stderr, "eliminated based on intensity: %d\n", eliminated_intensity);

  LASReader_Destroy(reader);
  LASHeader_Destroy(header);
  
  if (verbose)
  {
    fprintf(stderr, "x %.3f %.3f %.3f\n",LASPoint_GetX(surviving_point_min), LASPoint_GetX(surviving_point_max), LASPoint_GetX(surviving_point_max) - LASPoint_GetX(surviving_point_min));
    fprintf(stderr, "y %.3f %.3f %.3f\n",LASPoint_GetY(surviving_point_min), LASPoint_GetY(surviving_point_max), LASPoint_GetY(surviving_point_max) - LASPoint_GetY(surviving_point_min));
    fprintf(stderr, "z %.3f %.3f %.3f\n",LASPoint_GetZ(surviving_point_min), LASPoint_GetZ(surviving_point_max), LASPoint_GetZ(surviving_point_max) - LASPoint_GetZ(surviving_point_min));
    fprintf(stderr, "intensity %d %d %d\n",LASPoint_GetIntensity(surviving_point_min), LASPoint_GetIntensity(surviving_point_max), LASPoint_GetIntensity(surviving_point_max) - LASPoint_GetIntensity(surviving_point_min));
    fprintf(stderr, "edge_of_flight_line %d %d %d\n",LASPoint_GetFlightLineEdge(surviving_point_min), LASPoint_GetFlightLineEdge(surviving_point_max), LASPoint_GetFlightLineEdge(surviving_point_max) - LASPoint_GetFlightLineEdge(surviving_point_min));
    fprintf(stderr, "scan_direction_flag %d %d %d\n",LASPoint_GetScanDirection(surviving_point_min), LASPoint_GetScanDirection(surviving_point_max), LASPoint_GetScanDirection(surviving_point_max) - LASPoint_GetScanDirection(surviving_point_min));
    fprintf(stderr, "number_of_returns_of_given_pulse %d %d %d\n",LASPoint_GetNumberOfReturns(surviving_point_min), LASPoint_GetNumberOfReturns(surviving_point_max), LASPoint_GetNumberOfReturns(surviving_point_max) - LASPoint_GetNumberOfReturns(surviving_point_min));
    fprintf(stderr, "return_number %d %d %d\n",LASPoint_GetReturnNumber(surviving_point_min), LASPoint_GetReturnNumber(surviving_point_max), LASPoint_GetReturnNumber(surviving_point_max) - LASPoint_GetReturnNumber(surviving_point_min));
    fprintf(stderr, "classification %d %d %d\n",LASPoint_GetClassification(surviving_point_min), LASPoint_GetClassification(surviving_point_max), LASPoint_GetClassification(surviving_point_max) - LASPoint_GetClassification(surviving_point_min));
    fprintf(stderr, "scan_angle_rank %d %d %d\n",LASPoint_GetScanAngleRank(surviving_point_min), LASPoint_GetScanAngleRank(surviving_point_max), LASPoint_GetScanAngleRank(surviving_point_max) - LASPoint_GetScanAngleRank(surviving_point_min));
    fprintf(stderr, "user_data %d %d %d\n",LASPoint_GetUserData(surviving_point_min), LASPoint_GetUserData(surviving_point_max), LASPoint_GetUserData(surviving_point_max) - LASPoint_GetUserData(surviving_point_min));
    fprintf(stderr, "gps_time %.8f %.8f %.8f\n",LASPoint_GetTime(surviving_point_min), LASPoint_GetTime(surviving_point_max), LASPoint_GetTime(surviving_point_max) - LASPoint_GetTime(surviving_point_min));
/*
    fprintf(stderr, "point_source_ID %d %d %d\n",surviving_point_min.point_source_ID, surviving_point_max.point_source_ID, surviving_point_max.point_source_ID - surviving_point_min.point_source_ID);
*/
  }


  if (file_name_out == 0 && !olas && !olaz)
  {
    fprintf(stderr, "no output specified. exiting ...\n");
    exit(1);
  }


  if (file_name_in)
  {
      reader = LASReader_Create(file_name_in);
      if (!reader) { 
          fprintf(stderr, 
                  "Error! %d, %s, in method %s\n",
                  LASError_GetLastErrorNum(),
                  LASError_GetLastErrorMsg(),
                  LASError_GetLastErrorMethod()
                 ); 
          exit(-1);
      } 
  }
  else
  {
    fprintf(stderr, "ERROR: no input specified\n");
    usage();
  }


  header = LASReader_GetHeader(reader);
  if (!header) { 
      fprintf(stderr, 
              "Error! %d, %s, in method %s\n",
              LASError_GetLastErrorNum(),
              LASError_GetLastErrorMsg(),
              LASError_GetLastErrorMethod()
             ); 
      exit(-1);
  } 

/*
  if (file_name_out)
  {
    file_out = fopen(file_name_out, "wb");

  }
  else if (olas)
  {
    file_out = stdout;

  }
  else
  {
    fprintf (stderr, "ERROR: no output specified\n");
    usage();
  }
*/

  surviving_header = LASHeader_Copy(header);

  LASHeader_SetPointRecordsCount(header, surviving_number_of_point_records);

  for (i = 0; i < 5; i++) LASHeader_SetPointRecordsByReturnCount(header, i, surviving_number_of_points_by_return[i]);

  minx = LASPoint_GetX(surviving_point_min) * LASHeader_GetScaleX(surviving_header) + LASHeader_GetOffsetX(surviving_header);
  maxx = LASPoint_GetX(surviving_point_max) * LASHeader_GetScaleX(surviving_header) + LASHeader_GetOffsetX(surviving_header);

  miny = LASPoint_GetY(surviving_point_min) * LASHeader_GetScaleY(surviving_header) + LASHeader_GetOffsetY(surviving_header);
  maxy = LASPoint_GetY(surviving_point_max) * LASHeader_GetScaleY(surviving_header) + LASHeader_GetOffsetY(surviving_header);

  minz = LASPoint_GetZ(surviving_point_min) * LASHeader_GetScaleZ(surviving_header) + LASHeader_GetOffsetZ(surviving_header);
  maxz = LASPoint_GetZ(surviving_point_max) * LASHeader_GetScaleZ(surviving_header) + LASHeader_GetOffsetZ(surviving_header);

/*  if (remove_extra_header) surviving_header.offset_to_point_data = surviving_header.header_size;
*/

  fprintf(stderr, "second pass reading %d and writing %d points ...\n", LASHeader_GetPointRecordsCount(header), surviving_number_of_point_records);

  writer = LASWriter_Create(file_name_out, surviving_header);
  if (!writer) { 
      fprintf(stderr, 
              "Error! %d, %s, in method %s\n",
              LASError_GetLastErrorNum(),
              LASError_GetLastErrorMsg(),
              LASError_GetLastErrorMethod()
             ); 
      exit(-1);
  } 

/*

  if (!remove_extra_header)
  {
    for (unsigned int u = lasreader->header.header_size; u < lasreader->header.offset_to_point_data; u++)
    {
      fputc(fgetc(file_in),file_out);
    }
  }
*/


  p  = LASReader_GetNextPoint(reader);

  while (p) {

    if (last_only && LASPoint_GetReturnNumber(p) != LASPoint_GetNumberOfReturns(p))
    {
        p  = LASReader_GetNextPoint(reader);
        continue;
    }
    if (first_only && LASPoint_GetReturnNumber(p) != 1)
    {
        p  = LASReader_GetNextPoint(reader);
        continue;

    }
    if (clip_xy_min && (LASPoint_GetX(p) < clip_xy_min[0] || LASPoint_GetY(p) < clip_xy_min[1]))
    {
        p  = LASReader_GetNextPoint(reader);
        continue;
    }
    if (clip_xy_max && (LASPoint_GetX(p) > clip_xy_max[0] || LASPoint_GetY(p) > clip_xy_max[1]))
    {
        p  = LASReader_GetNextPoint(reader);
        continue;
    }
    if (elim_return && (elim_return & (1 << LASPoint_GetReturnNumber(p))))
    {
        p  = LASReader_GetNextPoint(reader);
        continue;
    }
    if (elim_scan_angle_above && (LASPoint_GetScanAngleRank(p) > elim_scan_angle_above || LASPoint_GetScanAngleRank(p) < -elim_scan_angle_above))
    {
        p  = LASReader_GetNextPoint(reader);
        continue;
    }
    if (elim_intensity_below && LASPoint_GetIntensity(p) < elim_intensity_below)
    {
        p  = LASReader_GetNextPoint(reader);
        continue;
    }
    LASWriter_WritePoint(writer,p);

    p  = LASReader_GetNextPoint(reader);
  }

  LASWriter_Destroy(writer);
  LASReader_Destroy(reader);
  LASHeader_Destroy(header);
  LASPoint_Destroy(surviving_point_max);
  LASPoint_Destroy(surviving_point_min);

  if (verbose) ptime("done.");

  return 0;
}