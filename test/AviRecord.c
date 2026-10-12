#include"avilib.h"
#include"CircularList.h"
#include"AudioInput.h"
#include"VideoInput.h"
#include"stdatomic.h"
DECLARE_LIST(AudioAviList)
DECLARE_LIST(VideoAviList)
/****************************************函数重载*******************************************/
int AIDataIxport(AudioInputFrame *frame)
{
    ListNode *Node = CircularListRequest(&AudioAviList);
    if (Node != NULL)
    {
        AudioInputFrame *AudioFrame = malloc(sizeof(AudioInputFrame));
        *AudioFrame = *frame;
        AudioFrame->Data = malloc(frame->Len);
        memcpy(AudioFrame->Data, frame->Data, frame->Len);
        Node->Data = AudioFrame;
        // printf("%s,%lu\n", __func__, frame->SeqNo);
        if (CircularListWrite(&AudioAviList, Node) == -1)
        {
            printf("AudioAviList Write Fail\n");
            free(AudioFrame->Data);
            free(AudioFrame);
        }
    }
    return 0;
}

int VideoStreamExport(struct video_stream *Stream)
{
    ListNode *Node = CircularListRequest(&VideoAviList);
    if (Node != NULL)
    {
        struct video_stream *stream = malloc(sizeof(struct video_stream));
        *stream = *Stream;
        stream->data = malloc(Stream->len);
        memcpy(stream->data, Stream->data, Stream->len);
        Node->Data = stream;
        // printf("%s,%lu\n", __func__, Stream->SeqNo);
        if (CircularListWrite(&VideoAviList, Node) == -1)
        {
            printf("VideoAviList Write Fail\n");
            free(stream->data);
            free(stream);
        }
    }
    return 0;
}

void AvVideoiRecord(char *FileName,int FrameNum)
{
    int TotalFrame = FrameNum;
    static atomic_int Recording = ATOMIC_VAR_INIT(0);
    if(atomic_load(&Recording))
    {
        return ;
    }
    printf("Start Record Avi Video!!!\n");
    atomic_store(&Recording ,1);
    avi_t *AVI = AVI_open_output_file(FileName);
    AVI_set_video(AVI, 1024, 600, 25, "H264");
    AVI_set_audio(AVI, 1, 16000, 16, WAVE_FORMAT_PCM, 128);
    CreateCircularList(&AudioAviList);
    AudioAviList.RecvBlock = ATOMIC_VAR_INIT(0);
    CreateCircularList(&VideoAviList);
    VideoAviList.RecvBlock = ATOMIC_VAR_INIT(0);
    while(FrameNum)
    {
        AudioInputFrame *Frame = NULL;
        if (CircularListRead(&AudioAviList, (void *)&Frame) != -1)
        {
            printf("Avi Audio Write %d Frame\n",TotalFrame - FrameNum);
            AVI_write_audio(AVI,(char *) Frame->Data, Frame->Len);
            free(Frame->Data);
            free(Frame);
        }
        struct video_stream *Stream = NULL;
        if (CircularListRead(&VideoAviList, (void *)&Stream) != -1)
        {
            FrameNum--;
            printf("Avi Video Write %d Frame\n",TotalFrame - FrameNum);
            AVI_write_frame(AVI, (char *)Stream->data, Stream->len,Stream->frame_type == FRAME_TYPE_I);
            free(Stream->data);
            free(Stream);
        }
    }
    AVI_close(AVI);
    atomic_store(&Recording ,0);
    printf("Finsh Record Avi Video!!!\n");
}