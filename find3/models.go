package api

import (
	"github.com/schollz/find3/server/main/src/database"
	"github.com/schollz/find3/server/main/src/models"
)

// Save will add battery data to the database
func SaveBatteryData(b models.BatteryData) (err error) {
	db, err := database.Open(b.Family)
	if err != nil {
		return
	}
	err = db.AddBattery(b)
	db.Close()
	if err != nil {
		return
	}
	return
}

func SaveAssetData(b models.AssetData) (err error) {
	db, err := database.Open(b.Family)
	if err != nil {
		return
	}
	err = db.AddAsset(b)
	db.Close()
	if err != nil {
		return
	}
	return
}
