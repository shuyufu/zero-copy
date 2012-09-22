#define _GNU_SOURCE
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib.h>

int map = 0;
void *data = NULL;

int log_blocks (int fd)
{
  int filedes [2];
  int ret;
  loff_t offset   = 0;
  size_t size     = 4096;
  size_t to_write = 0;

  ret = pipe (filedes);
  if (ret < 0)
    goto out;

  /* splice the file into the pipe (data in kernel memory). */
  to_write = size;
  while (to_write > 0)
  {
    ret = splice (fd, NULL, filedes[1], NULL, to_write, SPLICE_F_MORE | SPLICE_F_MOVE);
    if (ret < 0) {
      perror("Flag 1");
      goto pipe;
    }
    else
      to_write -= ret;
  }

  to_write = size;
  /* splice the data in the pipe (in kernel memory) into the file. */
  while (to_write > 0) {
    ret = splice (filedes[0], NULL, map, &offset, to_write, SPLICE_F_MORE | SPLICE_F_MOVE);
    if (ret < 0) {
      perror("Flag 2");
      goto pipe;
    }
    else
      to_write -= ret;
  }
  system("hexdump dummy.dat");
 pipe:
  close (filedes[0]);
  close (filedes[1]);
 out:
  if (ret < 0)
    return -errno;
  return 0;
}

static gboolean on_read (GIOChannel   *source,
                         GIOCondition  condition,
                         gpointer      data)
{
  gchar      buf[1024]   = {0};
  GIOStatus  io_ret      = G_IO_STATUS_NORMAL;
  gsize      bytes_read  = 0;
  GError    *local_error = NULL;
  gboolean   ret         = TRUE;
  gint       fd          = 0;
  gint       r           = 0;

  fd = g_io_channel_unix_get_fd (source);

  r = log_blocks (fd);
  if (0 != r)
  {
    g_warning("Failed: %s", strerror(-r));
  }
  g_warning("done");
  sleep(1);
#if 0
  io_ret = g_io_channel_read_chars (source,
                                    buf,
                                    sizeof (buf),
                                    &bytes_read,
                                    &local_error);
  if (local_error)
  {
    g_warning ("Failed to read buffer: %s", local_error->message);
    g_clear_error (&local_error);
  }

  switch (io_ret)
  {
    case G_IO_STATUS_ERROR:
    {
      g_error ("Error reading: %s", local_error->message);
      ret = FALSE;
      break;
    }
    case G_IO_STATUS_EOF:
    {
      g_message ("EOF");
      ret = FALSE;
    }
    default:
    {
      g_warning("%s", buf);
      break;
    }
  }
#endif
  return ret;
}

static void on_connected (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GSocketClient     *client      = NULL;
  GSocketConnection *connection  = NULL;
  GSocket           *socket      = NULL;
  gint               fd          = 0;
  GIOChannel        *channel     = NULL;
  GError            *local_error = NULL;

  client = G_SOCKET_CLIENT (source_object);

  connection = g_socket_client_connect_to_host_finish (client, res, &local_error);
  if (local_error)
  {
    g_warning("connect failed: %s", local_error->message);
    g_clear_error (&local_error);
    return;
  }

  socket  = g_socket_connection_get_socket (connection);
  fd      = g_socket_get_fd (socket);

  channel = g_io_channel_unix_new (fd);
  g_io_channel_set_encoding (channel, NULL, NULL);
  g_io_add_watch (channel, G_IO_IN, on_read, g_object_ref (connection));
}

int main (int argc, char *argv[])
{
  GMainLoop     *loop   = NULL;
  GSocketClient *client = NULL;

  g_type_init ();

  map = open("dummy.dat", O_RDWR | O_CREAT);
  if (-1 == map)
  {
    perror ("open");
    return 0;
  }

  data = mmap (NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE, map, 0);
  if (MAP_FAILED == data)
  {
    perror ("mmap");
    return 0;
  }

  client = g_socket_client_new ();

  g_socket_client_connect_to_host_async (client, "127.0.0.1", 8080, NULL, on_connected, client);

  loop = g_main_loop_new (NULL, FALSE);
  
  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  return 0;
}

