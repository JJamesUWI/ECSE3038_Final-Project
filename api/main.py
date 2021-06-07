from flask import Flask, request, jsonify, json
from flask_pymongo import PyMongo
from marshmallow import Schema, fields, ValidationError
from bson.json_util import dumps
from json import loads
from datetime import datetime

with open('store.txt') as f:
    upass = f.readline().strip()

uri_update1 = "mongodb+srv://{}:{}@ecse3038-cluster.renxl.mongodb.net/LAB3?retryWrites=true&w=majority".format(
    upass, upass)

app = Flask(__name__)
app.config["MONGO_URI"] = uri_update1
mongo = PyMongo(app)


class EmbeddedSch(Schema):
    patient_id = fields.String(required=True)
    position = fields.Integer(required=True)
    temperature = fields.Integer(required=True)


class PatientSch(Schema):
    fname = fields.String(required=True)
    lname = fields.String(required=True)
    age = fields.Integer(required=True, strict=True)
    patient_id = fields.String(required=True)


# POST /api/record
@app.route("/api/record", methods=["POST"])
def embedded_post():

    hold = request.json
    recordData = {
        "patient_id": hold["patient_id"],
        "position": hold["position"],
        "temperature": hold["temperature"]
    }
    try:
        R_Data = EmbeddedSch().load(recordData)
        R_Data["last_updated"] = datetime.now()
        recordUP = mongo.db.test.insert_one(R_Data).inserted_id
        # recordDN = mongo.db.recordData.find_one(PatientUP)
        # return loads(dumps(recordDN))
        return {
            "success": True,
            "msg": "Data Saved"
        }
    except ValidationError as ve:
        return ve.messages, 400


# GET /api/patient
@app.route("/api/patient")
def listPatients():
    patients = mongo.db.PatientData.find()
    pList = jsonify(loads(dumps(patients)))
    return pList


# POST /api/patient
@app.route("/api/patient", methods=["POST"])
def FE_post():

    hold = request.json()
    patientData = {
        "patient_id": hold["patient_id"],
        "fname": hold["fname"],
        "lname": hold["lname"],
        "age": hold["age"]
    }
    try:
        P_Data = PatientSch().load(patientData)
        PatientUP = mongo.db.recordData.insert_one(P_Data).inserted_id
        #PatientDN = mongo.db.recordData.find_one(PatientUP)
        # return loads(dumps(PatientDN))
        return {
            "success": True,
            "msg": "Data Saved"
        }
    except ValidationError as ve:
        return ve.messages, 400


# PATCH /api/patient/:id
@app.route('/api/patient/<ObjectId:id>', methods=["PATCH"])
def patient_patch(id):
    mongo.db.PatientData.update_one({"patient_id": id}, {"$set": request.json})
    pPatch = mongo.db.PatientData.find_one(id)
    return loads(dumps(pPatch))


# DELETE /api/patient/:id
@app.route('/api/patient/<ObjectId:id>', methods=["DELETE"])
def patient_delete(id):
    patientDEL = mongo.db.PatientData.delete_one({"patient_id": id})
    if patientDEL.deleted_count == 1:
        return {
            "success": True
        }
    else:
        return {
            "success": False
        }, 400


if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=5000)
