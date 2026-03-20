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
    winner_id: Optional[str]
    is_your_turn: bool
    moves: list[MoveResponse]


class MoveFailureResponse(BaseModel):
    failure_reason: str


class GameSummary(BaseModel):
    game_id: str
    opponent_username: Optional[str]
    creation_time: datetime
    total_moves: int
    winner_username: Optional[str]


class UserProfileResponse(BaseModel):
    username: str
    games: list[GameSummary]
    current_game_ids: list[str]


class MyUserProfileResponse(BaseModel):
    username: str
    user_id: str
    games: list[GameSummary]
    current_game_ids: list[str]


class GameListEntry(BaseModel):
    game_id: str
    p1_uname: str
    p2_uname: str
    total_moves: int
    is_complete: bool
    winner_index: int


class GameListResponse(BaseModel):
    games: list[GameListEntry]
