# Automatically generated by pb2py
import protobuf as p


class NEMMosaicSupplyChange(p.MessageType):
    FIELDS = {
        1: ('namespace', p.UnicodeType, 0),
        2: ('mosaic', p.UnicodeType, 0),
        3: ('type', p.UVarintType, 0),
        4: ('delta', p.UVarintType, 0),
    }
