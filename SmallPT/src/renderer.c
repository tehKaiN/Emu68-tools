/*
    Copyright � 2017, The AROS Development Team. All rights reserved.
    $Id$
*/

//#include <proto/exec.h>

#include <exec/tasks.h>
#include <exec/ports.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#include "renderer.h"
#include "support.h"

extern void render_tile(int w, int h, int samps, int tile_x, int tile_y, ULONG *buffer, struct Task *gfx);
extern void RenderTile(struct ExecBase *SysBase, struct MsgPort *masterPort, struct MsgPort **myPort);

struct Worker {
    struct Task *   task;
    struct MsgPort *port;
    char *          name;
};

struct MyMessage *AllocMyMessage(struct List *msgPool)
{
    struct MyMessage *msg = (struct MyMessage *)RemHead(msgPool);
    if (msg)
    {
        msg->mm_Message.mn_Length = sizeof(struct MyMessage);
    }

    return msg;
}

void FreeMyMessage(struct List *msgPool, struct MyMessage *msg)
{
    AddHead(msgPool, &msg->mm_Message.mn_Node);
}

void Renderer(struct ExecBase *ExecBase, struct MsgPort *ParentMailbox)
{
    ULONG width = 0;
    ULONG height = 0;
    ULONG *bitmap = NULL;
    struct MsgPort *guiPort = NULL;
    struct MsgPort *port;
    struct MyMessage *messages;
    struct MyMessage *deathMessage = NULL;
    ULONG numberOfCores = 0;
    ULONG maxIter = 0;
    int cores_alive = 0;
    int tasks_in = 0;
    int tasks_out = 0;
    int tasks_work = 0;
    ULONG workerStack = 0x100000 + maximal_ray_depth * 8192;
    int expl_mode = 0;

    //Printf("[SMP-Smallpt-Renderer] Renderer started, ParentMailBox = %p\n", ParentMailbox);

    port = CreateMsgPort();
    
    if (port)
    {
        struct Message startup;
        int stayAlive = TRUE;
        struct List workList;
        struct List doneList;
        struct List msgPool;
        struct tileWork *workPackages;
        struct Worker *workers;

        NewList(&workList);
        NewList(&doneList);
        NewList(&msgPool);

        /* Prepare initial message and wait for startup msg */
        startup.mn_Length = sizeof(startup);
        startup.mn_ReplyPort = port;
 
#if 0
        D(bug("[SMP-Smallpt-Renderer] Sending welcome msg to parent\n"));
#endif
        PutMsg(ParentMailbox, &startup);

        while(width == 0)
        {
            struct MyMessage *msg;
            
            WaitPort(port);

            while ((msg = (struct MyMessage *)GetMsg(port)))
            {
                if (msg->mm_Message.mn_Node.ln_Type == NT_REPLYMSG)
                {
#if 0
                    D(bug("[SMP-Smallpt-Renderer] Parent replied to welcome msg\n"));
#endif
                }
                else if (msg->mm_Message.mn_Length >= sizeof(struct Message))
                {
                    if (msg->mm_Type == MSG_STARTUP)
                    {
#if 0
                        D(bug("[SMP-Smallpt-Renderer] recieved startup message at %p\n", msg));
#endif
                        guiPort = msg->mm_Message.mn_ReplyPort;
                        width = msg->mm_Body.Startup.Width;
                        height = msg->mm_Body.Startup.Height;
                        bitmap = msg->mm_Body.Startup.ChunkyBM;
                        //mainTask = msg->mm_Message.mn_ReplyPort->mp_SigTask;
                        numberOfCores = msg->mm_Body.Startup.coreCount;
                        maxIter = msg->mm_Body.Startup.numberOfSamples;
                        expl_mode = msg->mm_Body.Startup.explicitMode;
                    }
                    ReplyMsg(&msg->mm_Message);
                }
            }
        }

#if 0
        D(bug("[SMP-Smallpt-Renderer] Bitmap size %dx%d, chunky buffer at %p\n", width, height, bitmap));
#endif
        ULONG tile_count = (width / TILE_SIZE) * (height / TILE_SIZE);

        tasks_in = tile_count;

        workPackages = AllocMem(tile_count * sizeof(struct tileWork), MEMF_ANY);
#if 0
        D(bug("[SMP-Smallpt-Renderer] Preparing work packages\n"));
#endif
        for (ULONG i=0; i < tile_count; i++)
        {
            workPackages[i].x = i % (width / TILE_SIZE);
            workPackages[i].y = i / (width / TILE_SIZE);
            AddTail((struct List *)&workList, (struct Node *)&workPackages[i].node);
        }

        messages = AllocMem(sizeof(struct MyMessage) * numberOfCores * 10, MEMF_PUBLIC | MEMF_CLEAR);
        for (int i=0; i < numberOfCores * 10; i++)
            FreeMyMessage(&msgPool, &messages[i]);
#if 0
        D(bug("[SMP-Smallpt-Renderer] creating %d workers\n", numberOfCores));
#endif
        workers = AllocMem(sizeof(struct Worker) * numberOfCores, MEMF_PUBLIC | MEMF_CLEAR);
#if 0
        D(bug("[SMP-Smallpt-Renderer] worker stack size : %d bytes\n", workerStack));
#endif
        for (ULONG i=0; i < numberOfCores; i++)
        {
            workers[i].name = AllocMem(30, MEMF_PUBLIC | MEMF_CLEAR);
            _sprintf(workers[i].name, "SMP-SmallPT Worker.#%03u", i);

            workers[i].task = NewCreateTask(TASKTAG_NAME,   (Tag)workers[i].name,
                                    TASKTAG_PRI,            0,
                                    TASKTAG_PC,             (Tag)RenderTile,
                                    TASKTAG_ARG1,           (Tag)ExecBase,
                                    TASKTAG_ARG2,           (Tag)port,
                                    TASKTAG_ARG3,           (Tag)&workers[i].port,
                                    TASKTAG_STACKSIZE,      workerStack,
                                    TAG_DONE);
        }
        cores_alive = numberOfCores;
#if 0
        D(bug("[SMP-Smallpt-Renderer] all set up, doing work\n"));
#endif
        while(stayAlive)
        {
            struct MyMessage *msg;
            WaitPort(port);

            while ((msg = (struct MyMessage *)GetMsg(port)))
            {
                if (msg->mm_Message.mn_Node.ln_Type == NT_REPLYMSG)
                {
                    if (msg->mm_Type == MSG_DIE)
                    {
#if 0
                        D(bug("[SMP-Smallpt-Renderer] death of smallpt task detected...\n"));
#endif
                        cores_alive--;

                        if (cores_alive == 0)
                        {
                            stayAlive = FALSE;
                            ReplyMsg(&deathMessage->mm_Message);
                        }
                    }
                    FreeMyMessage(&msgPool, msg);
                }
                else
                {
                    if (msg->mm_Type == MSG_DIE)
                    {
#if 0
                        D(bug("[SMP-Smallpt-Renderer] time to die...\n"));
#endif
                        for (ULONG i=0; i < numberOfCores; i++)
                        {
                            struct MyMessage *m = AllocMyMessage(&msgPool);
                            if (m)
                            {
                                m->mm_Type = MSG_DIE;
                                m->mm_Message.mn_ReplyPort = port;
#if 0
                                D(bug("[SMP-Smallpt-Renderer] telling task %s to shut down\n", workers[i].task->tc_Node.ln_Name));
#endif
                                PutMsg(workers[i].port, &m->mm_Message);
                            }
                        }
                        deathMessage = msg;
                    }
                    else if (msg->mm_Type == MSG_HUNGRY)
                    {
                        struct MsgPort *workerPort = msg->mm_Message.mn_ReplyPort;
                        ReplyMsg(&msg->mm_Message);

                        if (!IsListEmpty((struct List *)&workList))
                        {
                            struct MyMessage *m = AllocMyMessage(&msgPool);

                            if (m)
                            {
                                struct tileWork *work = (struct tileWork *)RemHead((struct List *)&workList);
                                tasks_in--;
                                tasks_work++;

                                m->mm_Type = MSG_RENDERTILE;
                                m->mm_Message.mn_ReplyPort = port;
                                m->mm_Body.RenderTile.tile = work;
                                m->mm_Body.RenderTile.buffer = bitmap;
                                m->mm_Body.RenderTile.guiPort = guiPort;
                                m->mm_Body.RenderTile.width = width;
                                m->mm_Body.RenderTile.height = height;
                                m->mm_Body.RenderTile.numberOfSamples = maxIter;
                                m->mm_Body.RenderTile.explicitMode = expl_mode;

                                PutMsg(workerPort, &m->mm_Message);
                            }
                        }
                    }
                    else if (msg->mm_Type == MSG_RENDERREADY)
                    {
                        struct tileWork *work = msg->mm_Body.RenderTile.tile;
                        ReplyMsg(&msg->mm_Message);
                        AddHead((struct List *)&doneList, (struct Node *)work);
                        tasks_out++;
                        tasks_work--;
                    }
                    if (deathMessage == NULL)
                    {
                        struct MyMessage *m = AllocMyMessage(&msgPool);
                        if (m)
                        {
                            m->mm_Type = MSG_STATS;
                            m->mm_Message.mn_ReplyPort = port;
                            m->mm_Body.Stats.tasksIn = tasks_in;
                            m->mm_Body.Stats.tasksOut = tasks_out;
                            m->mm_Body.Stats.tasksWork = tasks_work;
                            PutMsg(guiPort, &m->mm_Message);
                        }
                    }
                }
            }
        }

        FreeMem(workPackages, tile_count * sizeof(struct tileWork));
        FreeMem(messages, sizeof(struct MyMessage) * numberOfCores * 10);
        FreeMem(workers, sizeof(struct Worker) * numberOfCores);
        DeleteMsgPort(port);
    }
#if 0
    D(bug("[SMP-Smallpt-Renderer] goodbye!\n"));
#endif
}
