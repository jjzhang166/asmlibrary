/****************************************************************************
*						AAMLibrary
*			http://code.google.com/p/aam-library
* Copyright (c) 2008-2009 by GreatYao, all rights reserved.
****************************************************************************/

#include <fstream>
#include "AAM_Util.h"
#include "AAM_Shape.h"
#include "AAM_PDM.h"
#include "AAM_TDM.h"
#include "AAM_PAW.h"

using namespace std;

//============================================================================
void ReadCvMat(std::istream &is, CvMat* mat)
{
	assert(CV_MAT_TYPE(mat->type) == CV_64FC1);
	double* p = mat->data.db;
	is.read((char*)p, mat->rows*mat->cols*sizeof(double));
}

//============================================================================
void WriteCvMat(std::ostream &os, const CvMat* mat)
{
	assert(CV_MAT_TYPE(mat->type) == CV_64FC1);
	double* p = mat->data.db;
	os.write((char*)p, mat->rows*mat->cols*sizeof(double));
}

//============================================================================
// compare function for the qsort() call below
static int str_compare(const void *arg1, const void *arg2)
{
    return strcmp((*(std::string*)arg1).c_str(), (*(std::string*)arg2).c_str());
}

#ifdef WIN32
#include <direct.h>
#include <io.h>
file_lists AAM_Common::ScanNSortDirectory(const std::string &path, const std::string &extension)
{
    WIN32_FIND_DATA wfd;
    HANDLE hHandle;
    string searchPath, searchFile;
    file_lists vFilenames;
	int nbFiles = 0;
    
	searchPath = path + "/*" + extension;
	hHandle = FindFirstFile(searchPath.c_str(), &wfd);
	if (INVALID_HANDLE_VALUE == hHandle)
    {
		fprintf(stderr, "ERROR(%s, %d): Cannot find (*.%s)files in directory %s\n",
			__FILE__, __LINE__, extension.c_str(), path.c_str());
		exit(0);
    }
    do
    {
        //. or ..
        if (wfd.cFileName[0] == '.')
        {
            continue;
        }
        // if exists sub-directory
        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
	    }
        else//if file
        {
			searchFile = path + "/" + wfd.cFileName;
			vFilenames.push_back(searchFile);
			nbFiles++;
		}
    }while (FindNextFile(hHandle, &wfd));

    FindClose(hHandle);

	// sort the filenames
    qsort((void *)&(vFilenames[0]), (size_t)nbFiles, sizeof(string), str_compare);

    return vFilenames;
}

int AAM_Common::MkDir(const char* dirname)
{
	if(access(dirname, 0))	return mkdir(dirname);
	return 0;
}

#else

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 1024

static int match(const char* s1, const char* s2)
{
	int diff = strlen(s1) - strlen(s2);
	if(diff >= 0 && !strcmp(s1+diff, s2))
		return 1;
	return 0;
}

file_lists AAM_Common::ScanNSortDirectory(const std::string &path, const std::string &extension)
{
	struct dirent *d;
	DIR* dir;
	struct stat s;
	char fullpath[MAX_PATH];
	file_lists allfiles;
	int num = 0;

	dir = opendir(path.c_str());
	if(dir == NULL)	
	{
		 fprintf(stderr, "Can not open directory %s\n", path.c_str());
		 exit(0);
	}

	while(d = readdir(dir))
	{
		sprintf(fullpath, "%s/%s", path.c_str(), d->d_name);
		if(stat(fullpath, &s) != -1)
		{
			if(S_ISDIR(s.st_mode))
				continue;
			if(match(d->d_name, extension.c_str()))
			{
				allfiles.push_back(std::string(fullpath));
				num++;
			}
		}

	}
	closedir(dir);
	qsort((void*)&(allfiles[0]), size_t(num), sizeof(std::string), str_compare);
	return allfiles;
}

int AAM_Common::MkDir(const char* dirname)
{
	if(access(dirname, 0))	return mkdir(dirname, 0777);
	return 0;
}

#endif

//============================================================================
void AAM_Common::DrawPoints(IplImage* image, const AAM_Shape& Shape)
{
	for(int i = 0; i < Shape.NPoints(); i++)
	{
		cvCircle(image, cvPointFrom32f(Shape[i]), 3, CV_RGB(255, 0, 0));
	}
}


//============================================================================
void AAM_Common::DrawTriangles(IplImage* image, const AAM_Shape& Shape, const std::vector<std::vector<int> >&tris)
{
	int idx1, idx2, idx3;
	for(int i = 0; i < tris.size(); i++)
	{
		idx1 = tris[i][0]; 
		idx2 = tris[i][1];
		idx3 = tris[i][2];
		cvLine(image, cvPointFrom32f(Shape[idx1]), cvPointFrom32f(Shape[idx2]),
			CV_RGB(128,255,0));
		cvLine(image, cvPointFrom32f(Shape[idx2]), cvPointFrom32f(Shape[idx3]),
			CV_RGB(128,255,0));
		cvLine(image, cvPointFrom32f(Shape[idx3]), cvPointFrom32f(Shape[idx1]),
			CV_RGB(128,255,0));
	}
}

//============================================================================
static void DrawAppearanceRGBAChannel(const AAM_Shape& refShape, 
									const std::vector<std::vector<int> >& tri,
									const std::vector<std::vector<int> >& rect1,
									const std::vector<std::vector<int> >& rect2,
									const std::vector<int>& pixTri,
									const std::vector<double>& alpha,
									const std::vector<double>& belta,
									const std::vector<double>& gamma,
									const double* fastt, const AAM_Shape& Shape, IplImage* image)
{
	int x1, x2, y1, y2, idx1, idx2;
	int xby4, idxby3;
	int minx, miny, maxx, maxy;
	int tri_idx, v1, v2, v3;
	byte *pimg, *pimg2;
	int nPoints = Shape.NPoints();
	double alpha0, belta0, gamma0;
	double t = gettime;
	
	minx = Shape.MinX(); miny = Shape.MinY();
	maxx = Shape.MaxX(); maxy = Shape.MaxY();
	if(maxx-minx <= 2 || maxy-miny <= 2) 
		return;

	for(int y = miny; y < maxy; y++)
	{
		y1 = y-miny;
		pimg = (byte*)(image->imageData + image->widthStep*y);
		//pimg2 = pimg + (minx<<2);
		for(int x = minx; x < maxx; x++)
		{
			x1 = x-minx;
			idx1 = rect1[y1][x1];
			if(idx1 >= 0)
			{
				tri_idx = pixTri[idx1];
				v1 = tri[tri_idx][0];
				v2 = tri[tri_idx][1];
				v3 = tri[tri_idx][2];
				
				//if(v1 < 0 || v2 < 0 || v3 < 0 || v1 >= nPoints || v2 >= nPoints || v3 >= nPoints)
				//	continue;
		
				x2 = alpha[idx1]*refShape[v1].x + belta[idx1]*refShape[v2].x +  
					gamma[idx1]*refShape[v3].x;
				y2 = alpha[idx1]*refShape[v1].y + belta[idx1]*refShape[v2].y +  
					gamma[idx1]*refShape[v3].y;
				//alpha0 = alpha[idx1];
				//belta0 = belta[idx1];
				//gamma0 = gamma[idx1];
				//x2 = alpha0*refShape[v1].x + belta0*refShape[v2].x + gamma0*refShape[v3].x;
				//y2 = alpha0*refShape[v1].y + belta0*refShape[v2].y + gamma0*refShape[v3].y;

				if(y2 < 0 || x2 < 0) continue;	
				idx2 = rect2[y2][x2];		
				idxby3 = idx2 + (idx2<<1);	/* 3*idx2 */
				
				xby4 = x<<2;			/* 4*x ABGR */
				pimg[xby4+2] = fastt[idxby3  ];
				pimg[xby4+1] = fastt[idxby3+1];
				pimg[xby4  ] = fastt[idxby3+2];
				//*pimg2++ = fastt[idxby3+2];
				//*pimg2++ = fastt[idxby3+1];
				//*pimg2++ = fastt[idxby3  ];
				//pimg2++;
			}
			//else
			//	pimg2 += 4;
		}
	}

	LOGD("DrawAppearanceRGBA time cost %.3f\n", gettime-t);
}

static void DrawAppearanceRGBChannel(const AAM_Shape& refShape, 
									const std::vector<std::vector<int> >& tri,
									const std::vector<std::vector<int> >& rect1,
									const std::vector<std::vector<int> >& rect2,
									const std::vector<int>& pixTri,
									const std::vector<double>& alpha,
									const std::vector<double>& belta,
									const std::vector<double>& gamma,
									const double* fastt, const AAM_Shape& Shape, IplImage* image)
{
	int x1, x2, y1, y2, idx1, idx2;
	int xby3, idxby3;
	int minx, miny, maxx, maxy;
	int tri_idx, v1, v2, v3;
	byte* pimg;
	int nPoints = Shape.NPoints();
	
	minx = Shape.MinX(); miny = Shape.MinY();
	maxx = Shape.MaxX(); maxy = Shape.MaxY();
	if(maxx-minx <= 2 || maxy-miny <= 2) 
		return;

	for(int y = miny; y < maxy; y++)
	{
		y1 = y-miny;
		pimg = (byte*)(image->imageData + image->widthStep*y);
		for(int x = minx; x < maxx; x++)
		{
			x1 = x-minx;
			idx1 = rect1[y1][x1];
			if(idx1 >= 0)
			{
				tri_idx = pixTri[idx1];
				v1 = tri[tri_idx][0];
				v2 = tri[tri_idx][1];
				v3 = tri[tri_idx][2];
				
				//if(v1 < 0 || v2 < 0 || v3 < 0 || v1 >= nPoints || v2 >= nPoints || v3 >= nPoints)
				//	continue;
		
				x2 = alpha[idx1]*refShape[v1].x + belta[idx1]*refShape[v2].x +  
					gamma[idx1]*refShape[v3].x;
				y2 = alpha[idx1]*refShape[v1].y + belta[idx1]*refShape[v2].y +  
					gamma[idx1]*refShape[v3].y;

				if(y2 < 0 || x2 < 0) continue;					
				idx2 = rect2[y2][x2];		
				idxby3 = idx2 + (idx2<<1);	/* 3*idx2 */
				
				xby3 = x + (x<<1);			/* 3*x */
				pimg[xby3  ] = fastt[idxby3  ];
				pimg[xby3+1] = fastt[idxby3+1];
				pimg[xby3+2] = fastt[idxby3+2];
			}
		}
	}
}

void AAM_Common::DrawAppearance(IplImage*image, const AAM_Shape& Shape, 
				const CvMat* t, const AAM_PAW& paw, const AAM_PAW& refpaw)
{
	int x1, x2, y1, y2, idx1, idx2;
	int xby3, idxby3;
	int minx, miny, maxx, maxy;
	int tri_idx, v1, v2, v3;
	byte* pimg;
	double* fastt = t->data.db;
	int nChannel = image->nChannels;
	int nPoints = Shape.NPoints();
	const AAM_Shape& refShape = refpaw.__referenceshape;
	const std::vector<std::vector<int> >& tri = paw.__tri;
	const std::vector<std::vector<int> >& rect1 = paw.__rect;
	const std::vector<std::vector<int> >& rect2 = refpaw.__rect;
	const std::vector<int>& pixTri = paw.__pixTri;
	const std::vector<double>& alpha = paw.__alpha;
	const std::vector<double>& belta= paw.__belta;
	const std::vector<double>& gamma = paw.__gamma;

	if(nChannel == 4)
	{
		DrawAppearanceRGBAChannel(refShape, tri, rect1, rect2, pixTri, alpha, belta, gamma, fastt, Shape, image);
		return;
	}
	else if(nChannel == 3)
	{
		DrawAppearanceRGBChannel(refShape, tri, rect1, rect2, pixTri, alpha, belta, gamma, fastt, Shape, image);
		return;
	}
	
	minx = Shape.MinX(); miny = Shape.MinY();
	maxx = Shape.MaxX(); maxy = Shape.MaxY();
	for(int y = miny; y < maxy; y++)
	{
		y1 = y-miny;
		pimg = (byte*)(image->imageData + image->widthStep*y);
		for(int x = minx; x < maxx; x++)
		{
			x1 = x-minx;
			idx1 = rect1[y1][x1];
			if(idx1 >= 0)
			{
				tri_idx = pixTri[idx1];
				v1 = tri[tri_idx][0];
				v2 = tri[tri_idx][1];
				v3 = tri[tri_idx][2];
				
				if(v1 < 0 || v2 < 0 || v3 < 0 || v1 >= nPoints || v2 >= nPoints || v3 >= nPoints)
					continue;
		
				x2 = alpha[idx1]*refShape[v1].x + belta[idx1]*refShape[v2].x +  
					gamma[idx1]*refShape[v3].x;
				y2 = alpha[idx1]*refShape[v1].y + belta[idx1]*refShape[v2].y +  
					gamma[idx1]*refShape[v3].y;
				
				if(y2 < 0 || x2 < 0) continue;	
				idx2 = rect2[y2][x2];		
				idxby3 = idx2 + (idx2<<1);	/* 3*idx2 */
				
				if(nChannel == 4)
				{	
					xby3 = x<<2;			/* 4*x ABGR */
					pimg[xby3+2] = fastt[idxby3  ];
					pimg[xby3+1] = fastt[idxby3+1];
					pimg[xby3] = fastt[idxby3+2];
				}
				else if(nChannel == 3)
				{	
					xby3 = x + (x<<1);			/* 3*x */
					pimg[xby3  ] = fastt[idxby3  ];
					pimg[xby3+1] = fastt[idxby3+1];
					pimg[xby3+2] = fastt[idxby3+2];
				}
				else
				{
					pimg[x] = (fastt[idxby3]+fastt[idxby3+1]+fastt[idxby3+2])/3;
				}
			}
		}
	}
}

//===========================================================================
void AAM_Common::CheckShape(CvMat* s, int w, int h)
{
	double* fasts = s->data.db;
	int npoints = s->cols / 2;

	for(int i = 0; i < npoints; i++)
	{
		if(fasts[2*i] > w-1) fasts[2*i] = w-1;
		else if(fasts[2*i] < 0) fasts[2*i] = 0;
		
		if(fasts[2*i+1] > h-1) fasts[2*i+1] = h-1;
		else if(fasts[2*i+1] < 0) fasts[2*i+1] = 0;
	}
}
