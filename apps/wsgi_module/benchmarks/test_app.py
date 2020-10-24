from bottle import route, default_app

app = default_app()

data = {
    "id": 78874,
    "seriesName": "Firefly",
    "aliases": [
        "Serenity"
    ],
    "banner": "graphical/78874-g3.jpg",
    "seriesId": "7097",
    "status": "Ended",
    "firstAired": "2002-09-20",
    "network": "FOX (US)",
    "networkId": "",
    "runtime": "45",
    "genre": [
        "Drama",
        "Science-Fiction"
    ],
    "overview": "In the far-distant future, Captain Malcolm \"Mal\" Reynolds is a renegade former brown-coat sergeant, now turned smuggler & rogue, "
                "who is the commander of a small spacecraft, with a loyal hand-picked crew made up of the first mate, Zoe Warren; the pilot Hoban \"Wash\" Washburn; "
                "the gung-ho grunt Jayne Cobb; the engineer Kaylee Frye; the fugitives Dr. Simon Tam and his psychic sister River. "
                "Together, they travel the far reaches of space in search of food, money, and anything to live on.",
    "lastUpdated": 1486759680,
    "airsDayOfWeek": "",
    "airsTime": "",
    "rating": "TV-14",
    "imdbId": "tt0303461",
    "zap2itId": "EP00524463",
    "added": "",
    "addedBy": None,
    "siteRating": 9.5,
    "siteRatingCount": 472,
}


@route('/api')
def api():
    return data
