#ifndef mex_h
#define mex_h
#define matrix_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#if defined HAVE_UCHAR_H
# include <uchar.h>
#endif	/* HAVE_UCHAR_H */

typedef struct impl_info_tag *MEX_impl_info;

typedef struct mxArray_tag mxArray;

typedef void (*mxFunctionPtr) (int nlhs, mxArray *plhs[], int nrhs, mxArray *prhs[]);

typedef bool mxLogical;

#if defined __STDC_UTF_16__ && defined HAVE_UCHAR_H
typedef char16_t CHAR16_T;
#else
typedef uint16_t CHAR16_T;
#endif

typedef CHAR16_T mxChar;

typedef enum {
	mxUNKNOWN_CLASS = 0,
	mxCELL_CLASS,
	mxSTRUCT_CLASS,
	mxLOGICAL_CLASS,
	mxCHAR_CLASS,
	mxVOID_CLASS,
	mxDOUBLE_CLASS,
	mxSINGLE_CLASS,
	mxINT8_CLASS,
	mxUINT8_CLASS,
	mxINT16_CLASS,
	mxUINT16_CLASS,
	mxINT32_CLASS,
	mxUINT32_CLASS,
	mxINT64_CLASS,
	mxUINT64_CLASS,
	mxFUNCTION_CLASS,
	mxOPAQUE_CLASS,
	mxOBJECT_CLASS,
#if defined(_LP64)
	mxINDEX_CLASS = mxUINT64_CLASS,
#else
	mxINDEX_CLASS = mxUINT32_CLASS,
#endif
} mxClassID;

typedef enum {
	mxREAL,
	mxCOMPLEX
} mxComplexity;

typedef size_t mwSize;
typedef size_t mwIndex;
typedef ptrdiff_t mwSignedIndex;


extern void *mxMalloc(size_t n);
extern void *mxCalloc(size_t nmemb, size_t mbsz);
extern void mxFree(void *ptr);
extern void *mxRealloc(void *ptr, size_t size);

extern mxClassID mxGetClassID(const mxArray *pa);

extern void *mxGetData(const mxArray *pa);
extern void mxSetData(mxArray *pa, void *newdata);

extern bool mxIsNumeric(const mxArray *pa);
extern bool mxIsCell(const mxArray *pa);
extern bool mxIsLogical(const mxArray *pa);
extern bool mxIsChar(const mxArray *pa);
extern bool mxIsStruct(const mxArray *pa);
extern bool mxIsOpaque(const mxArray *pa);
extern bool mxIsFunctionHandle(const mxArray *pa);
extern bool mxIsComplex(const mxArray *pa);
extern bool mxIsSparse(const mxArray *pa);
extern bool mxIsDouble(const mxArray *pa);
extern bool mxIsSingle(const mxArray *pa);
extern bool mxIsInt8(const mxArray *pa);
extern bool mxIsUint8(const mxArray *pa);
extern bool mxIsInt16(const mxArray *pa);
extern bool mxIsUint16(const mxArray *pa);
extern bool mxIsInt32(const mxArray *pa);
extern bool mxIsUint32(const mxArray *pa);
extern bool mxIsInt64(const mxArray *pa);
extern bool mxIsUint64(const mxArray *pa);
extern bool mxIsEmpty(const mxArray *pa);
extern bool mxIsClass(const mxArray *pa, const char *name);

extern void *mxGetImagData(const mxArray *pa);
extern void mxSetImagData(mxArray *pa, void *newdata);

extern mwSize mxGetNumberOfDimensions(const mxArray *pa);
extern const mwSize *mxGetDimensions(const mxArray *pa);

extern size_t mxGetNumberOfElements(const mxArray *pa);

extern double *mxGetPr(const mxArray *pa);
extern void mxSetPr(mxArray *pa, double *pr);
extern double *mxGetPi(const mxArray *pa);
extern void mxSetPi(mxArray *pa, double  *pi);
extern mxChar *mxGetChars(const mxArray *pa);
extern int mxGetUserBits(const mxArray *pa);
extern void mxSetUserBits(mxArray *pa, int value);
extern double mxGetScalar(const mxArray *pa);
extern size_t mxGetM(const mxArray *pa);
extern void mxSetM(mxArray *pa, mwSize m);
extern size_t mxGetN(const mxArray *pa);
extern void mxSetN(mxArray *pa, mwSize n);
extern size_t mxGetElementSize(const mxArray *pa);
extern mxArray *mxGetCell(const mxArray *pa, mwIndex i);
extern void mxSetCell(mxArray *pa, mwIndex i, mxArray *value);
extern const char *mxGetFieldNameByNumber(const mxArray *pa, int n);
extern int mxGetFieldNumber(const mxArray *pa, const char *name);
extern mxArray *mxGetFieldByNumber(const mxArray *pa, mwIndex i, int fieldnum);
extern void mxSetFieldByNumber(mxArray *pa, mwIndex i, int fieldnum, mxArray *value);
extern mxArray *mxGetField(const mxArray *pa, mwIndex i, const char *fieldname);
extern void mxSetField(mxArray *pa, mwIndex i, const char *fieldname, mxArray *value);
extern mxArray *mxGetProperty(const mxArray *pa, const mwIndex i, const char *propname);
extern void mxSetProperty(mxArray *pa, mwIndex i, const char *propname, const mxArray *value);
extern void mxGetNChars(const mxArray *pa, char *buf, mwSize nChars);
extern int mxGetString(const mxArray *pa, char *buf, mwSize buflen);

extern mxArray *mxCreateNumericMatrix(mwSize m, mwSize n, mxClassID classid, mxComplexity flag);
extern int mxSetDimensions(mxArray *pa, const mwSize *pdims, mwSize ndims);
extern void mxDestroyArray(mxArray *pa);
extern mxArray *mxCreateNumericArray(mwSize ndim, const mwSize *dims, mxClassID classid, mxComplexity flag);
extern mxArray *mxCreateCharArray(mwSize ndim, const mwSize *dims);
extern mxArray *mxCreateDoubleMatrix(mwSize m, mwSize n, mxComplexity flag);
extern mxArray *mxCreateSparse(mwSize m, mwSize n, mwSize nzmax, mxComplexity flag);
extern mxArray *mxCreateDoubleScalar(double value);

extern char *mxArrayToString(const mxArray *pa);
extern mxArray *mxCreateStringFromNChars(const char *str, mwSize n);
extern mxArray *mxCreateString(const char *str);
extern mxArray *mxCreateCellMatrix(mwSize m, mwSize n);
extern mxArray *mxCreateCellArray(mwSize ndim, const mwSize *dims);
extern mxArray *mxCreateStructMatrix(size_t m, size_t n, int nfields, const char **fieldnames);
extern mxArray *mxCreateStructArray(mwSize ndim, const mwSize *dims, int nfields, const char **fieldnames);
extern mxArray *mxDuplicateArray(const mxArray *in);

extern int mxAddField(mxArray *pa, const char *fieldname);
extern void mxRemoveField(mxArray *pa, int field);

extern double mxGetEps(void);
extern double mxGetInf(void);
extern double mxGetNaN(void);

extern bool mxIsFinite(double x);
extern bool mxIsInf(double x);
extern bool mxIsNaN(double x);

/*
 * mexFunction is the user-defined C routine that is called upon invocation
 * of a MEX-function.
 */
void mexFunction(
    int           nlhs,           /* number of expected outputs */
    mxArray       *plhs[],        /* array of pointers to output arguments */
    int           nrhs,           /* number of inputs */
    const mxArray *prhs[]         /* array of pointers to input arguments */
);

extern void mexErrMsgTxt(const char *error_msg);
extern void mexErrMsgIdAndTxt(const char *identifier, const char *err_msg, ...);
extern void mexWarnMsgTxt(const char *warn_msg);
extern void mexWarnMsgIdAndTxt(const char *identifier, const char *warn_msg, ...);
extern int mexPrintf(const char	*fmt, ...);

#define printf mexPrintf

extern void mexMakeArrayPersistent(mxArray *pa);
extern void mexMakeMemoryPersistent(void *ptr);

extern int mexSet(double handle, const char *property, mxArray *value);
extern const mxArray *mexGet(double handle, const char *property);

extern void mexLock(void);
extern void mexUnlock(void);
extern bool mexIsLocked(void);
extern const char *mexFunctionName(void);

extern int mexAtExit(void(*exit_fcn)(void));

#endif /* mex_h */
