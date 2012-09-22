#include <gio/gio.h>
#include <glib.h>

static gboolean on_read (GIOChannel   *source,
                         GIOCondition  condition,
                         gpointer      data)
{
  gchar      buf[1024]   = {0};
  GIOStatus  ret         = G_IO_STATUS_NORMAL;
  gsize      bytes_read  = 0;
  GError    *local_error = NULL;
  
  ret = g_io_channel_read_chars (source,
                                 buf,
                                 sizeof (buf),
                                 &bytes_read,
                                 &local_error);
  if (local_error)
  {
    g_warning ("Failed to read buffer: %s", local_error->message);
    g_clear_error (&local_error);
  }

  switch (ret)
  {
    case G_IO_STATUS_ERROR:
    {
      g_error ("Error reading: %s", local_error->message);
      g_object_unref (data);
      return FALSE;
      break;
    }
    case G_IO_STATUS_EOF:
    {
      g_message ("EOF");
      g_object_unref (data);
      return FALSE;
    }
    default:
    {
      g_io_channel_write_chars (source, buf, bytes_read, NULL, NULL);
      break;
    }
  }

  return TRUE;
}

static gboolean on_write (GIOChannel   *source,
                         GIOCondition  condition,
                         gpointer      data)
{
  gchar      buf[4096];
  gsize      bytes_written = 0;
  GError    *local_error   = NULL;
  GIOStatus  ret           = G_IO_STATUS_NORMAL;

  ret = g_io_channel_write_chars (source,
                                  buf,
                                  sizeof (buf),
                                  &bytes_written,
                                  &local_error);
  if (local_error)
  {
    g_warning ("Failed to write buffer: %s", local_error->message);
    g_clear_error (&local_error);
    g_object_unref (data);
    return FALSE;
  }

  return TRUE;
}

static gboolean on_incoming (GSocketService    * service,
                             GSocketConnection * connection,
                             GObject           * source_object,
                             gpointer            user_data)
{
  GSocketAddress *socket_address = NULL;
  GInetAddress   *address        = NULL;
  guint16         port           = 0;

  GSocket    *socket   = NULL;
  gint        fd       = 0;
  GIOChannel *channel  = NULL;
  gboolean    ret      = TRUE;
  
  socket_address = g_socket_connection_get_remote_address (connection, NULL);

  address        = g_inet_socket_address_get_address (
      G_INET_SOCKET_ADDRESS (socket_address));

  port           = g_inet_socket_address_get_port (
      G_INET_SOCKET_ADDRESS (socket_address));

  g_debug ("New Connection from %s:%d",
      g_inet_address_to_string (address), port);

  socket  = g_socket_connection_get_socket (connection);

  fd      = g_socket_get_fd (socket);

  channel = g_io_channel_unix_new (fd);
  
  g_io_channel_set_encoding (channel, NULL, NULL);

  // g_io_add_watch (channel, G_IO_IN, on_read, g_object_ref (connection));

  g_io_add_watch (channel, G_IO_OUT, on_write, g_object_ref (connection));

  return ret;
}

int main (int argc, char * argv[])
{
  GMainLoop      *loop           = NULL;
  GInetAddress   *address        = NULL;
  GSocketAddress *socket_address = NULL;
  GSocketService *service        = NULL;
  GError         *local_error    = NULL;

  g_type_init ();

  address = g_inet_address_new_from_string("127.0.0.1");

  socket_address = g_inet_socket_address_new (address, 8080);

  service = g_socket_service_new ();

  g_signal_connect (G_OBJECT (service),
                    "incoming",
                    G_CALLBACK (on_incoming),
                    NULL);

  g_socket_listener_add_address (G_SOCKET_LISTENER (service),
                                 socket_address,
                                 G_SOCKET_TYPE_STREAM,
                                 G_SOCKET_PROTOCOL_TCP,
                                 NULL,
                                 NULL,
                                 &local_error);
  if (local_error)
  {
    g_warning ("Failed to add address: %s", local_error->message);
    g_clear_error (&local_error);
    return 0;
  }

  g_object_unref (socket_address);

  g_object_unref (address);

  g_socket_service_start (service);
  
  loop = g_main_loop_new (NULL, FALSE);
  
  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  g_socket_service_stop (service);

  return 0;
}

