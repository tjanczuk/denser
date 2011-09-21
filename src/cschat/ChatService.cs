using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ServiceModel;
using System.IO;
using System.ServiceModel.Web;
using System.Collections.ObjectModel;
using System.Threading;

namespace cschat
{
    class Entry
    {
        public int Id { get; set; }
        public string Message { get; set; }
        public string ContentType { get; set; }
    }

    [ServiceContract]
    [ServiceBehavior(InstanceContextMode = InstanceContextMode.Single)]
    class ChatService
    {
        Collection<Entry> entries = new Collection<Entry>();
        Collection<PollAsyncResult> polls = new Collection<PollAsyncResult>();
        int currentId = 0;
        Timer timer;

        public ChatService()
        {
            this.timer = new Timer(this.PulishDanteLine, null, 200, Timeout.Infinite);
        }

        void PulishDanteLine(object state)
        {
            try
            {
                this.PostMessage(new MemoryStream(Encoding.UTF8.GetBytes(Dante.GetNextLine())));
            }
            catch 
            {}
            this.timer.Change(200, Timeout.Infinite);
        }

        [WebGet(UriTemplate="")]
        public Stream GetUI()
        {
            WebOperationContext.Current.OutgoingResponse.ContentLength = ChatServiceUI.UI.Length;
            WebOperationContext.Current.OutgoingResponse.ContentType = "text/html";
            return new MemoryStream(ChatServiceUI.UI);
        }

        [WebInvoke(Method = "POST", UriTemplate = "/messages/")]
        public void PostMessage(Stream message)
        {
            byte[] buffer = new byte[4 * 1024];
            int offset = 0, len;
            while (0 != (len = message.Read(buffer, offset, buffer.Length - offset)) && (offset + len) < buffer.Length)
            {
                offset += len;
            }
            if (len != 0)
            {
                throw new InvalidOperationException("Too long message");
            }
            lock (this.entries)
            {
                this.entries.Add(new Entry { Id = ++this.currentId, Message = Encoding.UTF8.GetString(buffer, 0, offset), ContentType = "text/plain" });
                if (this.entries.Count > 50)
                {
                    this.entries.RemoveAt(0);
                }
                this.ReleaseAllPolls();
            }
        }

        [WebGet(UriTemplate = "/messages/?last={last}")]
        [OperationContract(AsyncPattern = true)]
        public IAsyncResult BeginPoll(int last, AsyncCallback callback, object state)
        {
            lock (this.entries)
            {
                int indexOfNextMessage = this.entries.Count > 0 ? last - this.entries[0].Id + 1 : 0;
                if (indexOfNextMessage < 0)
                {
                    indexOfNextMessage = 0;
                }
                if (indexOfNextMessage < this.entries.Count)
                {
                    PollAsyncResult result = new PollAsyncResult(state, callback);
                    result.Complete(this.entries[indexOfNextMessage], true);
                    return result;
                }
                else
                {
                    PollAsyncResult result = new PollAsyncResult(state, callback);
                    this.polls.Add(result);
                    return result;                    
                }
            }
        }

        public Stream EndPoll(IAsyncResult result)
        {
            ((PollAsyncResult)result).Close();
            Entry entry = ((PollAsyncResult)result).Entry;
            byte[] message;
            if (entry != null)
            {
                message = Encoding.UTF8.GetBytes(entry.Message);                
                WebOperationContext.Current.OutgoingResponse.ContentType = entry.ContentType;
                WebOperationContext.Current.OutgoingResponse.Headers["Content-ID"] = entry.Id.ToString();
            }
            else
            {
                message = new byte[0];
            }
            WebOperationContext.Current.OutgoingResponse.ContentLength = message.Length;
            WebOperationContext.Current.OutgoingResponse.Headers["Cache-Control"] = "no-cache";

            return new MemoryStream(message);
        }

        // called under lock
        void ReleaseAllPolls()
        {
            Entry entry = this.entries.Last();
            foreach (PollAsyncResult poll in this.polls)
            {
                poll.Complete(entry, false);
            }
            this.polls.Clear();
        }

        class PollAsyncResult : IAsyncResult
        {
            AsyncCallback callback;
            ManualResetEvent waitHandle;
            Timer timer;
            object syncRoot = new object();

            public PollAsyncResult(object state, AsyncCallback callback)
            {
                this.AsyncState = state;
                this.callback = callback;
                this.AsyncWaitHandle = this.waitHandle = new ManualResetEvent(false);
                this.timer = new Timer(this.Cancel, null, 15000, Timeout.Infinite);
            }

            void Cancel(object state)
            {
                this.Complete(null, false);
            }

            public void Complete(Entry entry, bool synchronous)
            {
                lock (this.syncRoot)
                {
                    if (!this.IsCompleted)
                    {
                        try
                        {
                            this.timer.Dispose();
                            this.timer = null;
                            this.Entry = entry;
                            this.waitHandle.Set();
                            this.IsCompleted = true;
                            this.CompletedSynchronously = synchronous;
                            this.callback(this);
                        }
                        catch (Exception)
                        {
                        }
                    }
                }
            }

            public void Close()
            {
                this.waitHandle.Dispose();
                if (this.timer != null)
                {
                    this.timer.Dispose();
                    this.timer = null;
                }
            }

            public Entry Entry { get; private set; }
            public object AsyncState { get; private set; }
            public WaitHandle AsyncWaitHandle { get; private set; }
            public bool CompletedSynchronously { get; private set; }
            public bool IsCompleted { get; private set; }
        }
    }
}
