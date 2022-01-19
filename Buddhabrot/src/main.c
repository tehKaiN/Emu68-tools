/*
    Copyright 2017-2018, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/lists.h>
#include <libraries/Picasso96.h>

#include <proto/Picasso96.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/alib_protos.h>
#include <clib/timer_protos.h>

#include "work.h"
#include "support.h"

CONST_STRPTR version = "$VER: Buddhabrot 1.0 (03.03.2017) �2017 The AROS Development Team";

#define ARG_TEMPLATE "SIZE/K/N,THREADS/K/N,MAXITER/K/N,OVERSAMPLE/K/N,BUDDHA/S"

enum {
    ARG_SIZE,
    ARG_MAXCPU,
    ARG_MAXITER,
    ARG_OVERSAMPLE,
    ARG_BUDDHA,


    ARG_COUNT
};

struct TimerBase *TimerBase = NULL;

int main()
{
    struct SMPMaster workMaster;
    ULONG args[ARG_COUNT] = { 0, 0, 0, 0 };
    int max_cpus = 0;
    ULONG coreCount = 1, core;
    ULONG signals;
    struct Screen *pubScreen;
    struct Window *displayWin;
    struct BitMap *outputBMap = NULL;
    struct MemList *coreML;
    struct SMPWorker *coreWorker;
    struct SMPWorkMessage *workMsg;
    ULONG rawArgs[2];
    char buffer[100];
    BOOL complete = FALSE, buddha = FALSE;
    ULONG maxWork = 0;
    ULONG oversample = 0;
    struct RDArgs *rda;
    struct Library *P96Base;

    ULONG req_size = 0;

    max_cpus = coreCount;

    struct MsgPort *timerPort = CreateMsgPort();
    struct timerequest *tr = CreateIORequest(timerPort, sizeof(struct timerequest));

    struct timeval start_time;
    struct timeval now;

    P96Base = OpenLibrary("Picasso96API.library", 0);
    if (P96Base == NULL) {
        Printf("Failed to open Picasso96API.library\n");
        return -1;
    }

    if (timerPort)
    {
        FreeSignal(timerPort->mp_SigBit);
        timerPort->mp_SigBit = -1;
        timerPort->mp_Flags = PA_IGNORE;
    }

    if (tr)
    {
        if (!OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)tr, 0))
        {
            TimerBase = (struct TimerBase *)tr->tr_node.io_Device;
        }
    } else return 0;

    rda = ReadArgs(ARG_TEMPLATE, args, NULL);
    if (rda != NULL)
    {
        LONG *ptr = (LONG *)args[ARG_MAXCPU];
        if (ptr)
            max_cpus = *ptr;

        ptr = (LONG *)args[ARG_SIZE];
        if (ptr)
            req_size = *ptr;

        ptr = (LONG *)args[ARG_MAXITER];
        if (ptr)
        {
            maxWork = *ptr;
            if (maxWork < 2)
                maxWork = 2;
            else if (maxWork > 10000000)
                maxWork = 10000000;
        }

        ptr = (LONG *)args[ARG_OVERSAMPLE];
        if (ptr)
        {
            oversample = *ptr;
            if (oversample < 1)
                oversample = 1;
            else if (oversample > 10)
                oversample = 10;
        }

        if (args[ARG_BUDDHA])
            buddha = TRUE;
    }

    coreCount = max_cpus;

    /* Create a port that workers/masters will signal us using .. */
    if ((workMaster.smpm_MasterPort = CreateMsgPort()) == NULL)
        return 0;

    NewList(&workMaster.smpm_Workers);

    Printf(version);
    Printf("Work Master MsgPort @ 0x%08lx\n", workMaster.smpm_MasterPort);
    Printf("SigTask = 0x%08lx\n", workMaster.smpm_MasterPort->mp_SigTask);

    workMaster.smpm_WorkerCount = 0;
    if (buddha)
    {
        workMaster.smpm_MaxWork = 4000;
        workMaster.smpm_Oversample = 4;
        workMaster.smpm_Buddha = TRUE;
    }
    else
    {
        workMaster.smpm_MaxWork = 1000;
        workMaster.smpm_Oversample = 1;
        workMaster.smpm_Buddha = FALSE;
    }

    if (maxWork)
        workMaster.smpm_MaxWork = maxWork;
    if (oversample && buddha)
        workMaster.smpm_Oversample = oversample;

    InitSemaphore(&workMaster.smpm_Lock);

    Printf("Initializing workers\n");

    for (core = 0; core < max_cpus; core++)
    {
        if ((coreML = AllocMem(sizeof(struct MemList) + sizeof(struct MemEntry), MEMF_PUBLIC|MEMF_CLEAR)) != NULL)
        {
            coreML->ml_ME[0].me_Length = sizeof(struct SMPWorker);
            if ((coreML->ml_ME[0].me_Addr = AllocMem(sizeof(struct SMPWorker), MEMF_PUBLIC|MEMF_CLEAR)) != NULL)
            {
                coreWorker = coreML->ml_ME[0].me_Addr;
                Printf("* CPU Worker Node allocated @ 0x%08lx\n", coreWorker);

                coreML->ml_ME[1].me_Length = 22;
                if ((coreML->ml_ME[1].me_Addr = AllocMem(22, MEMF_PUBLIC|MEMF_CLEAR)) != NULL)
                {
                    coreML->ml_NumEntries = 2;

                    _sprintf(coreML->ml_ME[1].me_Addr, "Worker #%d", core);
                    
                    Printf("* Worker Task Name '%s'\n", coreML->ml_ME[1].me_Addr);
                    Printf("* Worker Stack Size = %ld bytes\n", ((workMaster.smpm_MaxWork / 50000) + 1) * 40960);

                    coreWorker->smpw_MasterPort = workMaster.smpm_MasterPort;
                    coreWorker->smpw_Node.ln_Type = 0;
                    coreWorker->smpw_SyncTask = FindTask(NULL);
                    coreWorker->smpw_Lock = &workMaster.smpm_Lock;
                    coreWorker->smpw_MaxWork = workMaster.smpm_MaxWork;
                    coreWorker->smpw_Oversample = workMaster.smpm_Oversample;
                    coreWorker->smpw_Task = NewCreateTask(
                                        TASKTAG_NAME,       (Tag)coreML->ml_ME[1].me_Addr,
                                        TASKTAG_PRI,        (Tag)0,
                                        TASKTAG_PC,         (Tag)SMPTestWorker,
                                        TASKTAG_STACKSIZE,  (Tag)((workMaster.smpm_MaxWork / 50000) + 1) * 40960,
                                        TASKTAG_USERDATA,   (Tag)coreWorker,
                                        TAG_DONE);

                    Printf("* Worker Task: 0x%08lx\n", coreWorker->smpw_Task);

                    if (coreWorker->smpw_Task)
                    {
                        workMaster.smpm_WorkerCount++;
                        Wait(SIGBREAKF_CTRL_C);
                        AddTail(&workMaster.smpm_Workers, &coreWorker->smpw_Node);
                        AddHead(&coreWorker->smpw_Task->tc_MemEntry, &coreML->ml_Node);
                    }
                }
                else
                {
                    FreeMem(coreML->ml_ME[0].me_Addr, sizeof(struct SMPWorker));
                    FreeMem(coreML, sizeof(struct MemList) + sizeof(struct MemEntry));
                }
            }
            else
            {
                FreeMem(coreML, sizeof(struct MemList) + sizeof(struct MemEntry));
            }
        }
    }

    Printf("Waiting for workers to become ready ...\n");

    do {
        Delay(1);
        complete = TRUE;
        ForeachNode(&workMaster.smpm_Workers, coreWorker)
        {
            if (coreWorker->smpw_Node.ln_Type != 1)
            {
                complete = FALSE;
                break;
            }
        }
    } while (!complete);

    /*
        * We now have our workers all launched,
        * and a node for each on our list.
        * lets get them to do some work ...
        */
    Printf("Sending out work to do ...\n");

    _sprintf(buffer, "Buddhabrot fractal (%ld workers on %ld threads)", workMaster.smpm_WorkerCount, coreCount);

    complete = FALSE;
    pubScreen = LockPubScreen(0);

    if (req_size == 0)
        req_size = (pubScreen) ? ((pubScreen->Height - pubScreen->BarHeight) * 3) / 4 : 256;

    if ((displayWin = OpenWindowTags(0,
                                    WA_PubScreen, (Tag)pubScreen,
                                    WA_Left, 0,
                                    WA_Top, (pubScreen) ? pubScreen->BarHeight : 10,
                                    WA_InnerWidth, req_size,
                                    WA_InnerHeight, req_size,
                                    WA_Title, (Tag)buffer,
                                    WA_SimpleRefresh, TRUE,
                                    WA_CloseGadget, TRUE,
                                    WA_DepthGadget, TRUE,
                                    WA_DragBar, TRUE,
                                    WA_SizeGadget, FALSE,
                                    WA_SizeBBottom, FALSE,
                                    WA_SizeBRight, FALSE,
                                    WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
                                    TAG_DONE)) != NULL)
    {
        struct RastPort *outBMRastPort;
        BOOL working = TRUE;
        UWORD width, height;

        SetWindowPointer( displayWin, WA_BusyPointer, TRUE, TAG_DONE );

        if (pubScreen)
            UnlockPubScreen(0, pubScreen);

        width = workMaster.smpm_Width  = (displayWin->Width - displayWin->BorderLeft - displayWin->BorderRight);
        height = workMaster.smpm_Height = (displayWin->Height - displayWin->BorderTop - displayWin->BorderBottom);

        outputBMap = AllocBitMap(   workMaster.smpm_Width,
                                    workMaster.smpm_Height,
                                    GetBitMapAttr(displayWin->WScreen->RastPort.BitMap, BMA_DEPTH),
                                    BMF_DISPLAYABLE, displayWin->WScreen->RastPort.BitMap);

        workMaster.smpm_WorkBuffer = AllocMem(workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG), MEMF_ANY|MEMF_CLEAR);

        ULONG *rgba = AllocMem(workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG), MEMF_ANY|MEMF_CLEAR);

        struct RenderInfo ri;
        ri.Memory = rgba;
        ri.BytesPerRow = workMaster.smpm_Width * sizeof(ULONG);
        ri.RGBFormat = RGBFB_R8G8B8A8;

        Printf("Target BitMap @ 0x%08lx\n", outputBMap);
        Printf("    %dx%dx%d\n", workMaster.smpm_Width, workMaster.smpm_Height, GetBitMapAttr(outputBMap, BMA_DEPTH));
        Printf("Buffer @ 0x%08lx\n", workMaster.smpm_WorkBuffer);

        outBMRastPort = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_ANY);
        InitRastPort(outBMRastPort);
        outBMRastPort->BitMap = outputBMap;

        Printf("Target BitMap RastPort @ 0x%08lx\n", outBMRastPort);

        p96WritePixelArray(&ri, 0, 0, 
                            outBMRastPort,
                            0, 0,
                            workMaster.smpm_Width, workMaster.smpm_Height);

        BltBitMapRastPort (outputBMap, 0, 0,
                displayWin->RPort, displayWin->BorderLeft, displayWin->BorderTop,
                width, height, 0xC0); 

        workMaster.smpm_Master = NewCreateTask( TASKTAG_NAME,       (Tag)"Buddha Master",
                                                TASKTAG_PRI,        -1,
                                                TASKTAG_PC,         (Tag)SMPTestMaster,
                                                TASKTAG_USERDATA,   (Tag)&workMaster,
                                                TAG_DONE);

        GetSysTime(&start_time);

        Printf("Waiting for the work to be done ...\n");

        /* Wait for the workers to finish processing the data ... */
        
        while ((working) && ((signals = Wait(SIGBREAKF_CTRL_D | (1 << displayWin->UserPort->mp_SigBit))) != 0))
        {
            if ((signals & SIGBREAKF_CTRL_D) && (!complete))
            {
                complete = TRUE;

                /* Is work still being issued? */
                if (workMaster.smpm_Master)
                    complete = FALSE;

                /* Are workers still working ? */
                ForeachNode(&workMaster.smpm_Workers, coreWorker)
                {
                    if (!IsListEmpty(&coreWorker->smpw_MsgPort->mp_MsgList))
                        complete = FALSE;
                }

                for (ULONG i = 0; i < workMaster.smpm_Width * workMaster.smpm_Height; i++)
                {
                    ULONG rgb = workMaster.smpm_WorkBuffer[i];
                    ULONG r=96*rgb,g=128*rgb,b=256*rgb;

                    b = (b + 255) / 512;
                    r = (r + 255) / 512;
                    g = (g + 255) / 512;

                    if (b > 255) b = 255;
                    if (r > 255) r = 255;
                    if (g > 255) g = 255;

                    ULONG c = 0xff | (b << 8) | (g << 16) | (r << 24);
                    rgba[i] = c;
                }

                p96WritePixelArray(&ri, 0, 0, 
                                    outBMRastPort,
                                    0, 0,
                                    workMaster.smpm_Width, workMaster.smpm_Height);

                if (complete)
                {
                    SetWindowPointer( displayWin, WA_BusyPointer, FALSE, TAG_DONE );

                    GetSysTime(&now);
                    SubTime(&now, &start_time);
                    Printf("Fractal rendered in %ld:%02ld:%02ld\n",
                                                now.tv_secs / 3600,
                                                (now.tv_secs / 60) % 60,
                                                now.tv_secs % 60);

                    rawArgs[0] = coreCount;
                    _sprintf(buffer, "Buddhabrot fractal (0 workers on %ld threads) - Finished", coreCount);
                    SetWindowTitles( displayWin, buffer, NULL);
                }
            }
            else if (signals & (1 << displayWin->UserPort->mp_SigBit))
            {
                struct IntuiMessage *msg;
                while ((msg = (struct IntuiMessage *)GetMsg(displayWin->UserPort)))
                {
                    switch(msg->Class)
                    {
                        case IDCMP_CLOSEWINDOW:
                            working = FALSE;
                            break;

                        case IDCMP_REFRESHWINDOW:
                            BeginRefresh(msg->IDCMPWindow);
                            BltBitMapRastPort (outputBMap, 0, 0,
                                msg->IDCMPWindow->RPort, msg->IDCMPWindow->BorderLeft, msg->IDCMPWindow->BorderTop,
                                width, height, 0xC0);
                            EndRefresh(msg->IDCMPWindow, TRUE);
                            break;
                    }
                    ReplyMsg((struct Message *)msg);
                }
            }
            BltBitMapRastPort (outputBMap, 0, 0,
                displayWin->RPort, displayWin->BorderLeft, displayWin->BorderTop,
                width, height, 0xC0); 
        }


        Printf("Letting workers know we are finished ...\n");

        ForeachNode(&workMaster.smpm_Workers, coreWorker)
        {
            /* Tell the workers they are finished ... */
            if ((workMsg = (struct SMPWorkMessage *)AllocMem(sizeof(struct SMPWorkMessage), MEMF_CLEAR)) != NULL)
            {
                workMsg->smpwm_Type = SPMWORKTYPE_FINISHED;
                PutMsg(coreWorker->smpw_MsgPort, (struct Message *)workMsg);
            }
        }
        FreeMem(workMaster.smpm_WorkBuffer, workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG));
        FreeMem(rgba, workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG));
        CloseWindow(displayWin);
        outBMRastPort->BitMap = NULL;
        FreeMem(outBMRastPort, sizeof(struct RastPort));
        FreeBitMap(outputBMap);
    }
    if (pubScreen) UnlockPubScreen(0, pubScreen);

    Printf("Waiting for workers to finish ...\n");

    /* wait for the workers to finish up before we exit ... */
    if (workMaster.smpm_MasterPort)
    {
        while ((signals = Wait(SIGBREAKF_CTRL_C)) != 0)
        {
            if ((signals & SIGBREAKF_CTRL_C) && IsListEmpty(&workMaster.smpm_Workers))
                break;
        }
        DeleteMsgPort(workMaster.smpm_MasterPort);
    }

    Printf("Done ...\n");

    CloseLibrary(P96Base);

    return 0;
}
