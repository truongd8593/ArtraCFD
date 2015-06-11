/****************************************************************************
 * Export Computed Data in VTK format                                       *
 * Programmer: Huangrui Mo                                                  *
 * - Follow the Google's C/C++ style Guide.                                 *
 ****************************************************************************/
/****************************************************************************
 * Required Header Files
 ****************************************************************************/
#include "paraviewstream.h"
#include <stdio.h> /* standard library for input and output */
#include <string.h> /* manipulating strings */
#include "paraview.h"
#include "commons.h"
/****************************************************************************
 * Static Function Declarations
 ****************************************************************************/
static int InitializeTransientParaviewDataFile(ParaviewSet *);
static int WriteSteadyParaviewDataFile(ParaviewSet *, const Time *);
static int WriteParaviewVariableFile(const Real *U, ParaviewSet *,
        const Space *, const Partition *, const Flow *);
/****************************************************************************
 * Function definitions
 ****************************************************************************/
int WriteComputedDataParaview(const Real *U, const Space *space, 
        const Time *time, const Partition *part, const Flow *flow)
{
    ShowInformation("  writing field data to file...");
    ParaviewSet paraSet = { /* initialize ParaviewSet environment */
        .baseName = "paraview", /* data file base name */
        .fileName = {'\0'}, /* data file name */
        .floatType = "Float32", /* paraview data type */
        .byteOrder = "LittleEndian" /* byte order of data */
    };
    if (0 == time->stepCount) { /* this is the initialization step */
        InitializeTransientParaviewDataFile(&paraSet);
    }
    WriteSteadyParaviewDataFile(&paraSet, time);
    WriteParaviewVariableFile(U, &paraSet, space, part, flow);
    return 0;
}
static int InitializeTransientParaviewDataFile(ParaviewSet *paraSet)
{
    FILE *filePointer = fopen("paraview.pvd", "w");
    if (NULL == filePointer) {
        FatalError("failed to write data to transient case file...");
    }
    /* output information to file */
    fprintf(filePointer, "<?xml version=\"1.0\"?>\n");
    fprintf(filePointer, "<VTKFile type=\"Collection\" version=\"1.0\"\n");
    fprintf(filePointer, "         byte_order=\"%s\">\n", paraSet->byteOrder);
    fprintf(filePointer, "  <Collection>\n");
    fprintf(filePointer, "  </Collection>\n");
    fprintf(filePointer, "</VTKFile>\n");
    fclose(filePointer); /* close current opened file */
    return 0;
}
static int WriteSteadyParaviewDataFile(ParaviewSet *paraSet, const Time *time)
{
    /* store updated basename in filename */
    snprintf(paraSet->fileName, sizeof(ParaviewString), "%s%05d", 
            paraSet->baseName, time->outputCount); 
    /* basename is updated here! */
    snprintf(paraSet->baseName, sizeof(ParaviewString), "%s", paraSet->fileName); 
    /* current filename */
    snprintf(paraSet->fileName, sizeof(ParaviewString), "%s.pvd", paraSet->baseName); 
    FILE *filePointer = fopen(paraSet->fileName, "w");
    if (NULL == filePointer) {
        FatalError("failed to write data to steady case file...");
    }
    /* output information to file */
    fprintf(filePointer, "<?xml version=\"1.0\"?>\n");
    fprintf(filePointer, "<VTKFile type=\"Collection\" version=\"1.0\"\n");
    fprintf(filePointer, "         byte_order=\"%s\">\n", paraSet->byteOrder);
    fprintf(filePointer, "  <Collection>\n");
    fprintf(filePointer, "    <DataSet timestep=\"%.6g\" group=\"\" part=\"0\"\n", 
            time->currentTime);
    fprintf(filePointer, "             file=\"%s.vts\"/>\n", paraSet->baseName);
    fprintf(filePointer, "  </Collection>\n");
    fprintf(filePointer, "  <!-- Order %d -->\n", time->outputCount);
    fprintf(filePointer, "  <!-- Time %.6g -->\n", time->currentTime);
    fprintf(filePointer, "  <!-- Step %d -->\n", time->stepCount);
    fprintf(filePointer, "</VTKFile>\n");
    fclose(filePointer); /* close current opened file */
    /*
     * Add the current export to the transient case
     */
    filePointer = fopen("paraview.pvd", "r+");
    if (NULL == filePointer) {
        FatalError("failed to add data to transient file...");
    }
    /* seek the target line for adding information */
    char currentLine[200] = {'\0'}; /* store the current read line */
    int targetLine = 1;
    while (NULL != fgets(currentLine, sizeof currentLine, filePointer)) {
        CommandLineProcessor(currentLine); /* process current line */
        if (0 == strncmp(currentLine, "</Collection>", sizeof currentLine)) {
            break;
        }
        ++targetLine;
    }
    /* redirect to the target line */
    rewind(filePointer); /* seek to the beginning of the file */
    for (int line = 1; line < targetLine; ++line) {
        fgets(currentLine, sizeof currentLine, filePointer);
    }
    /* append informatiom */
    fprintf(filePointer, "    <DataSet timestep=\"%.6g\" group=\"\" part=\"0\"\n", 
            time->currentTime);
    fprintf(filePointer, "             file=\"%s.vts\"/>\n", paraSet->baseName);
    fprintf(filePointer, "  </Collection>\n");
    fprintf(filePointer, "</VTKFile>\n");
    fclose(filePointer); /* close current opened file */
    return 0;
}
static int WriteParaviewVariableFile(const Real *U, ParaviewSet *paraSet,
        const Space *space, const Partition *part, const Flow *flow)
{
    FILE *filePointer = NULL;
    snprintf(paraSet->fileName, sizeof(ParaviewString), "%s.vts", paraSet->baseName); 
    filePointer = fopen(paraSet->fileName, "w");
    if (NULL == filePointer) {
        FatalError("failed to write data file...");
    }
    int idx = 0; /* linear array index math variable */
    ParaviewReal data = 0.0; /* paraview scalar data */
    ParaviewReal vector[3] = {0.0}; /* paraview vector data elements */
    /* the scalar values at each node in current part */
    const char name[7][5] = {"rho", "u", "v", "w", "p", "T", "id"};
    int iMin = 0;
    int iMax = part->iSup[0] - 1 - part->iSub[0];
    int jMin = 0;
    int jMax = part->jSup[0] - 1 - part->jSub[0];
    int kMin = 0;
    int kMax = part->kSup[0] - 1 - part->kSub[0];
    fprintf(filePointer, "<?xml version=\"1.0\"?>\n");
    fprintf(filePointer, "<VTKFile type=\"StructuredGrid\" version=\"0.1\"\n");
    fprintf(filePointer, "         byte_order=\"%s\">\n", paraSet->byteOrder);
    fprintf(filePointer, "  <StructuredGrid WholeExtent=\"%d %d %d %d %d %d\">\n", 
            iMin, iMax, jMin, jMax, kMin, kMax);
    fprintf(filePointer, "    <Piece Extent=\"%d %d %d %d %d %d\">\n", 
            iMin, iMax, jMin, jMax, kMin, kMax);
    fprintf(filePointer, "      <PointData Scalars=\"rho\" Vectors=\"Vel\">\n");
    for (int dim = 0; dim < 7; ++dim) {
        fprintf(filePointer, "        <DataArray type=\"%s\" Name=\"%s\" format=\"ascii\">\n", 
                paraSet->floatType, name[dim]);
        fprintf(filePointer, "          ");
        for (int k = part->kSub[0]; k < part->kSup[0]; ++k) {
            for (int j = part->jSub[0]; j < part->jSup[0]; ++j) {
                for (int i = part->iSub[0]; i < part->iSup[0]; ++i) {
                    idx = IndexMath(k, j, i, space) * DIMU;
                    switch (dim) {
                        case 0: /* rho */
                            data = U[idx];
                            break;
                        case 1: /* u */
                            data = U[idx+1] / U[idx];
                            break;
                        case 2: /* v */
                            data = U[idx+2] / U[idx];
                            break;
                        case 3: /* w */
                            data = U[idx+3] / U[idx];
                            break;
                        case 4: /* p */
                            data = ComputePressure(idx, U, flow);
                            break;
                        case 5: /* T */
                            data = ComputeTemperature(idx, U, flow);
                            break;
                        case 6: /* node flag */
                            idx = idx / DIMU;
                            data = space->nodeFlag[idx];
                        default:
                            break;
                    }
                    fprintf(filePointer, "%.6g ", data);
                }
            }
        }
        fprintf(filePointer, "\n        </DataArray>\n");
    }
    fprintf(filePointer, "        <DataArray type=\"%s\" Name=\"Vel\"\n", paraSet->floatType);
    fprintf(filePointer, "                   NumberOfComponents=\"3\" format=\"ascii\">\n");
    fprintf(filePointer, "          ");
    for (int k = part->kSub[0]; k < part->kSup[0]; ++k) {
        for (int j = part->jSub[0]; j < part->jSup[0]; ++j) {
            for (int i = part->iSub[0]; i < part->iSup[0]; ++i) {
                idx = IndexMath(k, j, i, space) * DIMU;
                vector[0] = U[idx+1] / U[idx];
                vector[1] = U[idx+2] / U[idx];
                vector[2] = U[idx+3] / U[idx];
                fprintf(filePointer, "%.6g %.6g %.6g ", vector[0], vector[1], vector[2]);
            }
        }
    }
    fprintf(filePointer, "\n        </DataArray>\n");
    fprintf(filePointer, "      </PointData>\n");
    fprintf(filePointer, "      <CellData>\n");
    fprintf(filePointer, "      </CellData>\n");
    fprintf(filePointer, "      <Points>\n");
    fprintf(filePointer, "        <DataArray type=\"%s\" Name=\"points\"\n", paraSet->floatType);
    fprintf(filePointer, "                   NumberOfComponents=\"3\" format=\"ascii\">\n");
    fprintf(filePointer, "          ");
    for (int k = part->kSub[0]; k < part->kSup[0]; ++k) {
        for (int j = part->jSub[0]; j < part->jSup[0]; ++j) {
            for (int i = part->iSub[0]; i < part->iSup[0]; ++i) {
                vector[0] = ComputeX(i, space);
                vector[1] = ComputeY(j, space);
                vector[2] = ComputeZ(k, space);
                fprintf(filePointer, "%.6g %.6g %.6g ", vector[0], vector[1], vector[2]);
            }
        }
    }
    fprintf(filePointer, "\n        </DataArray>\n");
    fprintf(filePointer, "      </Points>\n");
    fprintf(filePointer, "    </Piece>\n");
    fprintf(filePointer, "  </StructuredGrid>\n");
    fprintf(filePointer, "</VTKFile>\n");
    fclose(filePointer); /* close current opened file */
    return 0;
}
/* a good practice: end file with a newline */
