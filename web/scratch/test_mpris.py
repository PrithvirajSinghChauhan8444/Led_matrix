import dbus

def get_mpris_metadata():
    try:
        bus = dbus.SessionBus()
        players = [name for name in bus.list_names() if name.startswith('org.mpris.MediaPlayer2.')]
        
        for player_name in players:
            player = bus.get_object(player_name, '/org/mpris/MediaPlayer2')
            iface = dbus.Interface(player, 'org.freedesktop.DBus.Properties')
            metadata = iface.Get('org.mpris.MediaPlayer2.Player', 'Metadata')
            
            title = metadata.get('xesam:title', 'Unknown Title')
            artist = metadata.get('xesam:artist', ['Unknown Artist'])[0]
            status = iface.Get('org.mpris.MediaPlayer2.Player', 'PlaybackStatus')
            
            return {
                "title": str(title),
                "artist": str(artist),
                "status": str(status)
            }
    except Exception as e:
        return {"error": str(e)}
    return None

if __name__ == "__main__":
    print(get_mpris_metadata())
