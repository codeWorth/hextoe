from datetime import datetime
from typing import Optional, Union

from pydantic import BaseModel


# -- Request schemas --

class UserInfoBody(BaseModel):
    username: str
    password: str


class UpdateUserBody(BaseModel):
    username: Optional[str] = None
    password: Optional[str] = None


class MoveBody(BaseModel):
    a: int
    r: int
    c: int


# -- Response schemas --

class MoveResponse(BaseModel):
    a: int
    r: int
    c: int
    p1: bool


class GameStateResponse(BaseModel):
    player_id_1: Optional[str]
    player_id_2: Optional[str]
    p1_uname: Optional[str]
    p2_uname: Optional[str]
    winner_id: Optional[str]
    is_your_turn: bool
    moves: list[MoveResponse]


class MoveFailureResponse(BaseModel):
    failure_reason: str


class GameListEntry(BaseModel):
    game_id: str
    is_complete: bool
    is_started: bool
    p1_uid: Optional[str]
    p2_uid: Optional[str]
    p1_uname: Optional[str]
    p2_uname: Optional[str]
    total_moves: int
    winner_index: int


class UserProfileResponse(BaseModel):
    username: str
    user_id: str


class GameListResponse(BaseModel):
    games: list[GameListEntry]
